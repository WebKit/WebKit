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

#import "config.h"
#import "TextCheckingController.h"

#if ENABLE(PLATFORM_DRIVEN_TEXT_CHECKING)

#import "AttributedString.h"
#import "TextCheckingControllerProxyMessages.h"
#import "WebProcessProxy.h"

namespace WebKit {

TextCheckingController::TextCheckingController(WebPageProxy& webPageProxy)
    : m_page(webPageProxy)
{
}

void TextCheckingController::replaceRelativeToSelection(NSAttributedString *annotatedString, int64_t selectionOffset, uint64_t length, uint64_t relativeReplacementLocation, uint64_t relativeReplacementLength)
{
    if (!m_page.hasRunningProcess())
        return;

    m_page.process().send(Messages::TextCheckingControllerProxy::ReplaceRelativeToSelection(annotatedString, selectionOffset, length, relativeReplacementLocation, relativeReplacementLength), m_page.pageID());
}

void TextCheckingController::removeAnnotationRelativeToSelection(NSString *annotationName, int64_t selectionOffset, uint64_t length)
{
    if (!m_page.hasRunningProcess())
        return;

    m_page.process().send(Messages::TextCheckingControllerProxy::RemoveAnnotationRelativeToSelection(annotationName, selectionOffset, length), m_page.pageID());
}

} // namespace WebKit

#endif // ENABLE(PLATFORM_DRIVEN_TEXT_CHECKING)
