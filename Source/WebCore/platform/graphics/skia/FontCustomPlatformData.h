/*
 * Copyright (C) 2007 Apple Computer, Inc.
 * Copyright (c) 2007, 2008, 2009, Google Inc. All rights reserved.
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

#ifndef FontCustomPlatformData_h
#define FontCustomPlatformData_h

#include "FontOrientation.h"
#include "FontRenderingMode.h"
#include "FontWidthVariant.h"
#include "TextOrientation.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>

#if OS(WINDOWS)
#include "PlatformString.h"
#include <windows.h>
#elif OS(LINUX) || OS(FREEBSD) || PLATFORM(BREWMP)
#include "SkTypeface.h"
#endif

namespace WebCore {

class FontPlatformData;
class SharedBuffer;

struct FontCustomPlatformData {
    WTF_MAKE_NONCOPYABLE(FontCustomPlatformData);
public:
#if OS(WINDOWS)
    FontCustomPlatformData(HANDLE fontReference, const String& name)
        : m_fontReference(fontReference)
        , m_name(name)
    {}
#elif OS(LINUX) || OS(FREEBSD) || PLATFORM(BREWMP)
    explicit FontCustomPlatformData(SkTypeface* typeface)
        : m_fontReference(typeface)
    {}
#endif

    ~FontCustomPlatformData();

    FontPlatformData fontPlatformData(int size, bool bold, bool italic, FontOrientation = Horizontal, TextOrientation = TextOrientationVerticalRight,
                                      FontWidthVariant = RegularWidth, FontRenderingMode = NormalRenderingMode);

    static bool supportsFormat(const String&);

#if OS(WINDOWS)
    HANDLE m_fontReference;
    String m_name;
#elif OS(LINUX) || OS(FREEBSD) || PLATFORM(BREWMP)
    SkTypeface* m_fontReference;
#endif
};

FontCustomPlatformData* createFontCustomPlatformData(SharedBuffer*);
}

#endif // FontCustomPlatformData_h
