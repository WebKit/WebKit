/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)

#include <wtf/CanMakeWeakPtr.h>
#include <wtf/CompletionHandler.h>

namespace WebCore {

class ChromeClient;
class HTMLElement;

template<typename> class ExceptionOr;

class ImmersiveElement : public CanMakeWeakPtr<ImmersiveElement> {
public:
    void ref() const { refImmersiveElement(); }
    void deref() const { derefImmersiveElement(); }

    virtual ~ImmersiveElement() = default;

    virtual HTMLElement& asHTMLElement() = 0;

    virtual bool isImmersiveEnabled() const = 0;
    virtual void prepareImmersivePresentation(CompletionHandler<void(ExceptionOr<void>)>&&) = 0;
    virtual void presentImmersivePresentation(ChromeClient&, CompletionHandler<void(bool)>&&) = 0;
    virtual void exitImmersivePresentation(CompletionHandler<void()>&& completion) { completion(); }

private:
    virtual void refImmersiveElement() const = 0;
    virtual void derefImmersiveElement() const = 0;
};

} // namespace WebCore

#endif // ENABLE(MODEL_ELEMENT_IMMERSIVE)
