/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Noncopyable.h>
#include <wtf/RetainPtr.h>

#if PLATFORM(IOS_FAMILY)

OBJC_CLASS UITraitCollection;

namespace WebCore {

// This class automatically saves and restores the current UITraitCollection for
// functions which call out into UIKit and rely on the current UITraitCollection being set
class LocalCurrentTraitCollection {
    WTF_MAKE_NONCOPYABLE(LocalCurrentTraitCollection);

public:
    WEBCORE_EXPORT LocalCurrentTraitCollection(bool useDarkAppearance, bool useElevatedUserInterfaceLevel);
    WEBCORE_EXPORT LocalCurrentTraitCollection(UITraitCollection *);
    WEBCORE_EXPORT ~LocalCurrentTraitCollection();

    bool usingDarkAppearance() const
    {
#if HAVE(OS_DARK_MODE_SUPPORT)
        return m_usingDarkAppearance;
#else
        return false;
#endif
    }

    bool usingElevatedUserInterfaceLevel() const
    {
#if HAVE(OS_DARK_MODE_SUPPORT)
        return m_usingElevatedUserInterfaceLevel;
#else
        return false;
#endif
    }

private:
#if HAVE(OS_DARK_MODE_SUPPORT)
    RetainPtr<UITraitCollection> m_savedTraitCollection;
    bool m_usingDarkAppearance { false };
    bool m_usingElevatedUserInterfaceLevel { false };
#endif
};

}

#endif // PLATFORM(IOS_FAMILY)

