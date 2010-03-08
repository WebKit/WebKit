/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SecureTextInput.h"

#if USE(CARBON_SECURE_INPUT_MODE)
#import <Carbon/Carbon.h>
#endif

namespace WebCore {

#if USE(CARBON_SECURE_INPUT_MODE)

#ifdef BUILDING_ON_TIGER
const short enableRomanKeyboardsOnly = -23;
#endif

void enableSecureTextInput()
{
    if (IsSecureEventInputEnabled())
        return;
    EnableSecureEventInput();
#ifdef BUILDING_ON_TIGER
    KeyScript(enableRomanKeyboardsOnly);
#else
    // WebKit substitutes nil for input context when in password field, which corresponds to null TSMDocument. So, there is
    // no need to call TSMGetActiveDocument(), which may return an incorrect result when selection hasn't been yet updated
    // after focusing a node.
    CFArrayRef inputSources = TISCreateASCIICapableInputSourceList();
    TSMSetDocumentProperty(0, kTSMDocumentEnabledInputSourcesPropertyTag, sizeof(CFArrayRef), &inputSources);
    CFRelease(inputSources);
#endif
}

void disableSecureTextInput()
{
    if (!IsSecureEventInputEnabled())
        return;
    DisableSecureEventInput();
#ifdef BUILDING_ON_TIGER
    KeyScript(smKeyEnableKybds);
#else
    TSMRemoveDocumentProperty(0, kTSMDocumentEnabledInputSourcesPropertyTag);
#endif
}

#endif // USE(CARBON_SECURE_INPUT_MODE)

} // namespace WebCore
