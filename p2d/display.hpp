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

#include <algorithm>
#include <p2d/bitmap.hpp>
#include <p2d/macro.hpp>
#include <p2d/result.hpp>

namespace p2d {

class P2D_PUBLIC Display {
 public:
  using pixel_t = Bitmap::pixel_t;

  Display() { source_.fill(0); }

  // Transfers the bitmap into the output buffer sent to
  // the push display. The push display buffer has a larger stride
  // as the given bitmap

  Result Flip(const Bitmap &g) {
    assert(g.Height() == kDataSourceHeight);

    const Bitmap::buffer_t buffer = g.Data();
    for (uint8_t line = 0; line < kDataSourceHeight; line++) {
      std::copy_n(buffer.begin() + line * g.Width(), g.Width(), source_.begin() + line * kDataSourceWidth);
    }

    return Result::NoError;
  }

  const pixel_t *Data() const { return source_.data(); }

  const pixel_t *Line(uint8_t line) const { return &source_[kDataSourceWidth * line]; }

 private:
  static const int kDataSourceWidth = 1024;
  static const int kDataSourceHeight = Bitmap::kHeight;

  std::array<pixel_t, kDataSourceWidth * kDataSourceHeight> source_;
};

}  // namespace p2d