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

#include <stdint.h>

#include <array>
#include <p2d/macro.hpp>

namespace p2d {

class P2D_PUBLIC Bitmap {
 public:
  using pixel_t = uint16_t;

  static const int16_t kWidth = 960;
  static const int16_t kHeight = 160;
  static const std::size_t kBufferSize = kWidth * kHeight;

  using buffer_t = std::array<pixel_t, kBufferSize>;

  Bitmap() { buffer_.fill(0); }

  inline int Width() const { return kWidth; }

  inline int Height() const { return kHeight; }

  inline static pixel_t PackPixel(unsigned char r, unsigned char g, unsigned char b) {
    pixel_t pixel = (b & 0xF8) >> 3;
    pixel <<= 6;
    pixel += (g & 0xFC) >> 2;
    pixel <<= 5;
    pixel += (r & 0xF8) >> 3;
    return pixel;
  }

  const buffer_t &Data() const { return buffer_; }

 private:
  std::array<pixel_t, kBufferSize> buffer_;
};

}  // namespace p2d