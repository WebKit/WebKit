/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "ValidationMessageClientImpl.h"

#include "Element.h"
#include "FrameView.h"
#include "RenderObject.h"
#include "WebTextDirection.h"
#include "WebViewClient.h"

using namespace WebCore;

namespace WebKit {

ValidationMessageClientImpl::ValidationMessageClientImpl(WebViewClient& client)
    : m_client(client)
    , m_currentAnchor(0)
    , m_timer(this, &ValidationMessageClientImpl::hideCurrentValidationMessage)
{
}

PassOwnPtr<ValidationMessageClientImpl> ValidationMessageClientImpl::create(WebViewClient& client)
{
    return adoptPtr(new ValidationMessageClientImpl(client));
}

ValidationMessageClientImpl::~ValidationMessageClientImpl()
{
    if (m_currentAnchor)
        hideValidationMessage(*m_currentAnchor);
}

void ValidationMessageClientImpl::showValidationMessage(const Element& anchor, const String& message)
{
    if (message.isEmpty()) {
        hideValidationMessage(anchor);
        return;
    }
    if (!anchor.renderBox())
        return;
    if (m_currentAnchor)
        hideValidationMessage(*m_currentAnchor);
    m_currentAnchor = &anchor;
    IntRect anchorInScreen = anchor.document()->view()->contentsToScreen(anchor.pixelSnappedBoundingBox());
    WebTextDirection dir = anchor.renderer()->style()->direction() == RTL ? WebTextDirectionRightToLeft : WebTextDirectionLeftToRight;
    m_client.showValidationMessage(anchorInScreen, message, anchor.fastGetAttribute(HTMLNames::titleAttr), dir);

    const double minimumSecondToShowValidationMessage = 5.0;
    const double secondPerCharacter = 0.05;
    m_timer.startOneShot(std::max(minimumSecondToShowValidationMessage, message.length() * secondPerCharacter));
}

void ValidationMessageClientImpl::hideValidationMessage(const Element& anchor)
{
    if (!m_currentAnchor || !isValidationMessageVisible(anchor))
        return;
    m_timer.stop();
    m_client.hideValidationMessage();
    m_currentAnchor = 0;
}

bool ValidationMessageClientImpl::isValidationMessageVisible(const Element& anchor)
{
    return m_currentAnchor == &anchor;
}

void ValidationMessageClientImpl::hideCurrentValidationMessage(Timer<ValidationMessageClientImpl>*)
{
    ASSERT(m_currentAnchor);
    hideValidationMessage(*m_currentAnchor);
}

}
