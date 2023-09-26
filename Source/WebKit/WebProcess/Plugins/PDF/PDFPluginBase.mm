/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "PDFPluginBase.h"

#if ENABLE(PDFKIT_PLUGIN) || ENABLE(UNIFIED_PDF)

#include "PluginView.h"
#include "WebFrame.h"
#include <WebCore/Document.h>
#include <WebCore/Frame.h>
#include <WebCore/HTMLPlugInElement.h>
#include <WebCore/PluginDocument.h>

namespace WebKit {
using namespace WebCore;

PDFPluginBase::PDFPluginBase(HTMLPlugInElement& element)
    : m_frame(*WebFrame::fromCoreFrame(*element.document().frame()))
{
}

void PDFPluginBase::setView(PluginView& view)
{
    ASSERT(!m_view);
    m_view = view;
}

void PDFPluginBase::destroy()
{
    ASSERT(!m_isBeingDestroyed);
    SetForScope scope { m_isBeingDestroyed, true };

    m_hasBeenDestroyed = true;
    m_documentFinishedLoading = true;

    teardown();

    m_view = nullptr;
}

bool PDFPluginBase::isFullFramePlugin() const
{
    // <object> or <embed> plugins will appear to be in their parent frame, so we have to
    // check whether our frame's widget is exactly our PluginView.
    if (!m_frame || !m_frame->coreLocalFrame())
        return false;

    RefPtr document = m_frame->coreLocalFrame()->document();
    if (!is<PluginDocument>(document))
        return false;
    return downcast<PluginDocument>(*document).pluginWidget() == m_view;
}

} // namespace WebKit

#endif // ENABLE(PDFKIT_PLUGIN) || ENABLE(UNIFIED_PDF)
