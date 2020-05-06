/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "AlternativeTextContextController.h"
#import <wtf/RetainPtr.h>

OBJC_CLASS NSTextAlternatives;
OBJC_CLASS NSView;

namespace WebCore {

class FloatRect;

class AlternativeTextUIController {
    WTF_MAKE_FAST_ALLOCATED;
public:
    AlternativeTextUIController() = default;

    WEBCORE_EXPORT uint64_t addAlternatives(NSTextAlternatives *); // Returns a context ID.

    WEBCORE_EXPORT void clear();

    using AcceptanceHandler = void (^)(NSString *);
    WEBCORE_EXPORT void showAlternatives(NSView *, const FloatRect& boundingBoxOfPrimaryString, uint64_t context, AcceptanceHandler);

    void WEBCORE_EXPORT removeAlternatives(uint64_t context);

    WEBCORE_EXPORT Vector<String> alternativesForContext(uint64_t context);

private:
#if USE(APPKIT)
    void handleAcceptedAlternative(NSString *, uint64_t context, NSTextAlternatives *);
    void dismissAlternatives();

    RetainPtr<NSView> m_view;
#endif
    AlternativeTextContextController m_contextController;
};

} // namespace WebCore
