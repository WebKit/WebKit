/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ImageSource.h"

namespace WebCore {

class ImageSourceSkia : public ImageSource {
public:
    // FIXME: This class is a hack to support Chromium's ICO decoder
    // Currently our ICO decoder decodes all data during setData() instead of
    // being lazy.  In addition, it only decodes one frame (closest to the size
    // passed to the decoder during createDecoder, called from setData) and
    // discards all other data in the file.
    //
    // To fix this will require fixing the ICO decoder to be lazy, or to decode
    // all frames.  Apple's decoders (ImageIO) decode all frames, and return
    // them all sorted in decreasing size.  WebCore always draws the first frame.
    //
    // This is a special-purpose routine for the favicon decoder, which is used
    // to specify a particular icon size for the ICOImageDecoder to prefer
    // decoding.  Note that not all favicons are ICOs, so this won't
    // necessarily do anything differently than ImageSource::setData().
    //
    // Passing an empty IntSize for |preferredIconSize| here is exactly
    // equivalent to just calling ImageSource::setData().  See also comments in
    // ICOImageDecoder.cpp.
    void setData(SharedBuffer* data,
                 bool allDataReceived,
                 const IntSize& preferredIconSize);
};

}
