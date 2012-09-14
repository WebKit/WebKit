/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "config.h"
#include "TestShell.h"

#include "linux/WebFontRendering.h"
#include "tests/ForwardIOStreamsAndroid.h"
#include "third_party/skia/include/ports/SkTypeface_android.h"

namespace {

// Must be same as DEVICE_DRT_DIR in Tools/Scripts/webkitpy/layout_tests/port/chromium_android.py.
#define DEVICE_DRT_DIR "/data/local/tmp/drt/"

const char fontMainConfigFile[] = DEVICE_DRT_DIR "android_main_fonts.xml";
const char fontFallbackConfigFile[] = DEVICE_DRT_DIR "android_fallback_fonts.xml";
const char fontsDir[] = DEVICE_DRT_DIR "fonts/";

} // namespace

void platformInit(int* argc, char*** argv)
{
    // Initialize skia with customized font config files.
    SkUseTestFontConfigFile(fontMainConfigFile, fontFallbackConfigFile, fontsDir);

    // Set up IO stream forwarding if necessary.
    WebKit::maybeInitIOStreamForwardingForAndroid(argc, argv);

    // Disable auto hint and use normal hinting in layout test mode to produce the same font metrics as chromium-linux.
    WebKit::WebFontRendering::setAutoHint(false);
    WebKit::WebFontRendering::setHinting(SkPaint::kNormal_Hinting);
}
