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

#pragma once

#include <assert.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <p2d/bitmap.hpp>
#include <p2d/display.hpp>
#include <p2d/macro.hpp>
#include <p2d/result.hpp>
#include <thread>

// Forward declarations. This avoid having to include libusb.h here
struct libusb_transfer;
struct libusb_device_handle;

namespace p2d {
class P2D_PUBLIC Driver {
 public:
  using pixel_t = Bitmap::pixel_t;

  // The display frame size is 960*160*2=300k, but we use 64 extra filler
  // pixels per line so that we get exactly 2048 bytes per line. The purpose
  // is that the device receives exactly 4 buffers of 512 bytes each per line,
  // so that the line boundary (which is where we save to SDRAM) does not fall
  // into the middle of a received buffer. Therefore we actually send
  // 1024*160*2=320k bytes per frame.

  static const int kLineSize = 2048;  // total line size
  static const int kLineCountPerSendBuffer = 8;

  // The data sent to the display is sliced into chunks of kLineCountPerSendBuffer
  // and we use kSendBufferCount buffers to communicate so we can prepare the next
  // request without having to wait for the current one to be finished
  // The sent buffer size (kSendBufferSize) must a multiple of these 2k per line,
  // and is selected for optimal performance.

  static const int kSendBufferCount = 3;
  static const int kSendBufferSize = kLineCountPerSendBuffer * kLineSize;  // buffer length in bytes

  Driver();
  ~Driver();

  Result Open(std::unique_ptr<Display>& source);

  void Swap(std::unique_ptr<Display>& source);

 private:
  static Result _FindPushDisplayDeviceHandle(libusb_device_handle** pHandle);
  static libusb_transfer* _AllocateAndPrepareTransferChunk(libusb_device_handle* handle, Driver* instance,
                                                           unsigned char* buffer, int bufferSize, uint8_t flags);
  static void _OnTransferFinished(libusb_transfer* transfer);

  Result _TransferStart();
  Result _TransferNextSlice(libusb_transfer* transfer);
  void _TransferFinished(libusb_transfer* transfer);

  // void _PollUsbForEvents();
  void _TransferThread();

  std::mutex sourceMutex_;
  std::unique_ptr<Display> source_;

  libusb_device_handle* handle_;
  libusb_transfer* frameHeaderTransfer_;
  // std::thread pollThread_;
  std::thread transferThread_;
  uint8_t currentLine_;
  std::atomic<bool> terminate_;
  unsigned char sendBuffers_[kSendBufferCount * kSendBufferSize];
};
}  // namespace p2d
