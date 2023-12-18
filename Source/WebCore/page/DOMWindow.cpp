/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DOMWindow.h"

#include "Document.h"
#include "Frame.h"
#include "HTTPParsers.h"
#include "Location.h"
#include "SecurityOrigin.h"
#include "WebCoreOpaqueRoot.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(DOMWindow);

DOMWindow::DOMWindow(GlobalWindowIdentifier&& identifier)
    : m_identifier(WTFMove(identifier))
{
}

DOMWindow::~DOMWindow() = default;

ExceptionOr<RefPtr<SecurityOrigin>> DOMWindow::createTargetOriginForPostMessage(const String& targetOrigin, Document& sourceDocument)
{
    RefPtr<SecurityOrigin> targetSecurityOrigin;
    if (targetOrigin == "/"_s)
        targetSecurityOrigin = &sourceDocument.securityOrigin();
    else if (targetOrigin != "*"_s) {
        targetSecurityOrigin = &SecurityOrigin::createFromString(targetOrigin).leakRef();
        // It doesn't make sense target a postMessage at an opaque origin
        // because there's no way to represent an opaque origin in a string.
        if (targetSecurityOrigin->isOpaque())
            return Exception { ExceptionCode::SyntaxError };
    }
    return targetSecurityOrigin;
}

Location& DOMWindow::location()
{
    if (!m_location)
        m_location = Location::create(*this);
    return *m_location;
}

RefPtr<Frame> DOMWindow::protectedFrame() const
{
    return frame();
}

WebCoreOpaqueRoot root(DOMWindow* window)
{
    return WebCoreOpaqueRoot { window };
}

} // namespace WebCore
