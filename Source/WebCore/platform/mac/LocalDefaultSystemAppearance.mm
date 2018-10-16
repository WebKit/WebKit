/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LocalDefaultSystemAppearance.h"

#if USE(APPKIT)

#include <AppKit/NSAppearance.h>

namespace WebCore {

LocalDefaultSystemAppearance::LocalDefaultSystemAppearance(bool useDarkAppearance)
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400
    m_savedSystemAppearance = [NSAppearance currentAppearance];
    m_usingDarkAppearance = useDarkAppearance;

    [NSAppearance setCurrentAppearance:[NSAppearance appearanceNamed:m_usingDarkAppearance ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua]];
#else
    UNUSED_PARAM(useDarkAppearance);
#endif
}

LocalDefaultSystemAppearance::~LocalDefaultSystemAppearance()
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400
    [NSAppearance setCurrentAppearance:m_savedSystemAppearance.get()];
#endif
}

}

#endif // USE(APPKIT)
