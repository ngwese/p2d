//
// Copyright (c) 2021 Greg Wuller
// Copyright (c) 2017 Ableton AG, Berlin
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//

#include <libusb.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <p2d/driver.hpp>

using namespace p2d;

//------------------------------------------------------------------------------

Driver::Driver() : source_(nullptr), handle_(nullptr) {}

//------------------------------------------------------------------------------

Driver::~Driver() {
  // shutdown the polling thread
  terminate_ = true;
  // if (pollThread_.joinable())
  // {
  //   pollThread_.join();
  // }
  if (transferThread_.joinable()) {
    transferThread_.join();
  }

  // TODO: cancel/wait for unfinished transfers?
  // TODO: close USB device
}

//------------------------------------------------------------------------------

Result Driver::Open(std::unique_ptr<Display>& source) {
  source_.swap(source);

  // Initialise the handle
  Result result = _FindPushDisplayDeviceHandle(&handle_);
  RETURN_IF_FAILED_MESSAGE(result, "Failed to initialize handle");
  assert(handle_ != nullptr);

  // Initialise the transfer
  // result = _TransferStart();
  // RETURN_IF_FAILED_MESSAGE(result, "Failed to initiate send");

  // We initiate a thread so we can receive events from libusb
  terminate_ = false;
  // pollThread_ = std::thread(&Driver::PollUsbForEvents, this);
  transferThread_ = std::thread(&Driver::_TransferThread, this);

  return Result::NoError;
}

void Driver::Swap(std::unique_ptr<Display>& source) {
  std::lock_guard<std::mutex> guard(sourceMutex_);
  source_.swap(source);
}

//------------------------------------------------------------------------------

Result Driver::_FindPushDisplayDeviceHandle(libusb_device_handle** pHandle) {
  int errorCode;

  // Initialises the library
  if ((errorCode = libusb_init(nullptr)) < 0) {
    return Result("Failed to initialize usblib");
  }

  // This is deprecated?
  // libusb_set_debug(nullptr, LIBUSB_LOG_LEVEL_ERROR);

  // Get a list of connected devices
  libusb_device** devices;
  ssize_t count;
  count = libusb_get_device_list(nullptr, &devices);
  if (count < 0) {
    return Result("could not get usb device list");
  }

  // Look for the one matching push2's decriptors
  libusb_device* device;
  libusb_device_handle* device_handle = nullptr;

  char ErrorMsg[256];

  // set message in case we get to the end of the list w/o finding a device
  sprintf(ErrorMsg, "display device not found\n");

  for (int i = 0; (device = devices[i]) != nullptr; i++) {
    struct libusb_device_descriptor descriptor;
    if ((errorCode = libusb_get_device_descriptor(device, &descriptor)) < 0) {
      sprintf(ErrorMsg, "could not get usb device descriptor, error: %d", errorCode);
      continue;
    }

    const uint16_t kAbletonVendorID = 0x2982;
    const uint16_t kPush2ProductID = 0x1967;

    if (descriptor.bDeviceClass == LIBUSB_CLASS_PER_INTERFACE && descriptor.idVendor == kAbletonVendorID &&
        descriptor.idProduct == kPush2ProductID) {
      if ((errorCode = libusb_open(device, &device_handle)) < 0) {
        sprintf(ErrorMsg, "could not open device, error: %d", errorCode);
      } else if ((errorCode = libusb_claim_interface(device_handle, 0)) < 0) {
        sprintf(ErrorMsg, "could not claim device with interface 0, error: %d", errorCode);
        libusb_close(device_handle);
        device_handle = nullptr;
      } else {
        break;  // successfully opened
      }
    }
  }

  *pHandle = device_handle;
  libusb_free_device_list(devices, 1);

  return device_handle ? Result::NoError : Result(ErrorMsg);
}

//------------------------------------------------------------------------------

// Allocate a libusb_transfer mapped to a transfer buffer. It also sets
// up the callback needed to communicate the transfer is done
libusb_transfer* Driver::_AllocateAndPrepareTransferChunk(libusb_device_handle* handle, Driver* instance,
                                                          unsigned char* buffer, int bufferSize, uint8_t flags) {
  // Allocate a transfer structure
  libusb_transfer* transfer = libusb_alloc_transfer(0);
  if (!transfer) {
    return nullptr;
  }

  // Sets the transfer characteristic
  const unsigned char kPush2BulkEPOut = 0x01;

  transfer->flags = flags;

  libusb_fill_bulk_transfer(transfer, handle, kPush2BulkEPOut, buffer, bufferSize, Driver::_OnTransferFinished,
                            instance, 1000);
  return transfer;
}

//------------------------------------------------------------------------------

void Driver::_OnTransferFinished(libusb_transfer* transfer) {
  static_cast<Driver*>(transfer->user_data)->_TransferFinished(transfer);
}

Result Driver::_TransferStart() {
  // Prevent the source from changing while all of the transfers are being queued up
  std::lock_guard<std::mutex> guard(sourceMutex_);

  currentLine_ = 0;

  // Allocates a transfer struct for the frame header
  static unsigned char frameHeader[16] = {0xFF, 0xCC, 0xAA, 0x88, 0x00, 0x00, 0x00, 0x00,
                                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  frameHeaderTransfer_ = _AllocateAndPrepareTransferChunk(handle_, this, frameHeader, sizeof(frameHeader), 0);

  for (int i = 0; i < kSendBufferCount; i++) {
    unsigned char* buffer = (sendBuffers_ + i * kSendBufferSize);

    // Allocates a transfer struct for the send buffers
    libusb_transfer* transfer =
        _AllocateAndPrepareTransferChunk(handle_, this, buffer, kSendBufferSize, LIBUSB_TRANSFER_FREE_TRANSFER);

    // Start a request for this buffer
    Result result = _TransferNextSlice(transfer);
    RETURN_IF_FAILED(result);
  }

  return Result::NoError;
}

//------------------------------------------------------------------------------

Result Driver::_TransferNextSlice(libusb_transfer* transfer) {
  // Start of a new frame, so send header first
  if (currentLine_ == 0) {
    if (libusb_submit_transfer(frameHeaderTransfer_) < 0) {
      return Result("could not submit frame header transfer");
    }
  }

  // Copy the next slice of the source data (represented by currentLine_)
  // to the transfer buffer

  unsigned char* dst = transfer->buffer;
  const char* src = reinterpret_cast<const char*>(source_->Data()) + kLineSize * currentLine_;
  std::memcpy(dst, src, kSendBufferSize);

  // Send it
  if (libusb_submit_transfer(transfer) < 0) {
    return Result("could not submit display data transfer");
  }

  // Update slice position
  currentLine_ += kLineCountPerSendBuffer;

  if (currentLine_ >= 160)  // FIXME: Replace with Display::kDataSourceHeight or Bitmap::kHeight
  {
    currentLine_ = 0;
  }

  return Result::NoError;
}

//------------------------------------------------------------------------------

void Driver::_TransferFinished(libusb_transfer* transfer) {
  if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
    switch (transfer->status) {
      case LIBUSB_TRANSFER_ERROR:
        printf("transfer failed\n");
        break;
      case LIBUSB_TRANSFER_TIMED_OUT:
        printf("transfer timed out\n");
        break;
      case LIBUSB_TRANSFER_CANCELLED:
        printf("transfer was cancelled\n");
        break;
      case LIBUSB_TRANSFER_STALL:
        printf("endpoint stalled/control request not supported\n");
        break;
      case LIBUSB_TRANSFER_NO_DEVICE:
        printf("device was disconnected\n");
        break;
      case LIBUSB_TRANSFER_OVERFLOW:
        printf("device sent more data than requested\n");
        break;
      default:
        printf("snd transfer failed with status: %d\n", transfer->status);
        break;
    }
  } else if (transfer->length != transfer->actual_length) {
    printf("only transferred %d of %d bytes\n", transfer->actual_length, transfer->length);
  }
}

//------------------------------------------------------------------------------

// void Driver::_PollUsbForEvents()
// {
//   static struct timeval timeout_500ms = {0 , 500000};
//   int terminate_main_loop = 0;

//   while (!terminate_main_loop && !terminate_.load())
//   {
//     if (libusb_handle_events_timeout_completed(nullptr, &timeout_500ms, &terminate_main_loop) < 0)
//     {
//       assert(0);
//     }
//   }
// }

void Driver::_TransferThread() {
  static struct timeval zeroTimeout = {0, 0};
  int terminateTransfers = 0;

  while (!terminateTransfers && !terminate_.load()) {
    // (re)transfer current frame buffer

    // TODO: only send the current frame if it
    // is new or 1 sec has elapsed w/ no change (push2 firmware blanks the
    // display after 2 sec with no transfer)
    Result result = _TransferStart();
    if (result.Failed()) {
      printf("Failed to initiate send");
      break;
    }

    int status = libusb_handle_events_timeout_completed(nullptr, &zeroTimeout, &terminateTransfers);
    if (status < 0) {
      printf("handle_events_timeout: failed with: %d", status);
    }

    // TODO: sleep until next display update interval (@60 fps)
    // FIXME: calculate wakeup time based on delta from before _TransferStart
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(16ms);
  }
}
