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

#include "config.h"
#include "PDFPluginBase.h"

#if ENABLE(LEGACY_PDFKIT_PLUGIN) || ENABLE(UNIFIED_PDF)

#include "PluginView.h"
#include "WebFrame.h"
#include <CoreFoundation/CoreFoundation.h>
#include <WebCore/ArchiveResource.h>
#include <WebCore/Document.h>
#include <WebCore/Frame.h>
#include <WebCore/HTMLPlugInElement.h>
#include <WebCore/LoaderNSURLExtras.h>
#include <WebCore/PluginDocument.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SharedBuffer.h>

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

void PDFPluginBase::ensureDataBufferLength(uint64_t targetLength)
{
    if (!m_data)
        m_data = adoptCF(CFDataCreateMutable(0, 0));

    auto currentLength = CFDataGetLength(m_data.get());
    ASSERT(currentLength >= 0);
    if (targetLength > (uint64_t)currentLength)
        CFDataIncreaseLength(m_data.get(), targetLength - currentLength);
}

void PDFPluginBase::streamDidReceiveResponse(const ResourceResponse& response)
{
    m_suggestedFilename = response.suggestedFilename();
    if (m_suggestedFilename.isEmpty())
        m_suggestedFilename = suggestedFilenameWithMIMEType(nil, "application/pdf"_s);
    if (!m_suggestedFilename.endsWithIgnoringASCIICase(".pdf"_s))
        m_suggestedFilename = makeString(m_suggestedFilename, ".pdf"_s);
}

void PDFPluginBase::streamDidReceiveData(const SharedBuffer& buffer)
{
    if (!m_data)
        m_data = adoptCF(CFDataCreateMutable(0, 0));

    ensureDataBufferLength(m_streamedBytes + buffer.size());
    memcpy(CFDataGetMutableBytePtr(m_data.get()) + m_streamedBytes, buffer.data(), buffer.size());
    m_streamedBytes += buffer.size();

    incrementalPDFStreamDidReceiveData(buffer);
}

void PDFPluginBase::streamDidFinishLoading()
{
    if (m_hasBeenDestroyed)
        return;

    addArchiveResource();

    m_documentFinishedLoading = true;

    if (!incrementalPDFStreamDidFinishLoading()) {
        createPDFDocument();
        installPDFDocument();
    }

    tryRunScriptsInPDFDocument();
}

void PDFPluginBase::streamDidFail()
{
    m_data = nullptr;
    incrementalPDFStreamDidFail();
}

void PDFPluginBase::addArchiveResource()
{
    // FIXME: It's a hack to force add a resource to DocumentLoader. PDF documents should just be fetched as CachedResources.

    // Add just enough data for context menu handling and web archives to work.
    NSDictionary* headers = @{ @"Content-Disposition": (NSString *)m_suggestedFilename, @"Content-Type" : @"application/pdf" };
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:m_view->mainResourceURL() statusCode:200 HTTPVersion:(NSString*)kCFHTTPVersion1_1 headerFields:headers]);
    ResourceResponse synthesizedResponse(response.get());

    auto resource = ArchiveResource::create(SharedBuffer::create(m_data.get()), m_view->mainResourceURL(), "application/pdf"_s, String(), String(), synthesizedResponse);
    m_view->frame()->document()->loader()->addArchiveResource(resource.releaseNonNull());
}

void PDFPluginBase::geometryDidChange(const IntSize& pluginSize, const AffineTransform& pluginToRootViewTransform)
{
    m_size = pluginSize;
    m_rootViewToPluginTransform = valueOrDefault(pluginToRootViewTransform.inverse());
}

void PDFPluginBase::invalidateRect(const IntRect& rect)
{
    if (!m_view)
        return;

    m_view->invalidateRect(rect);
}

IntPoint PDFPluginBase::convertFromRootViewToPlugin(const IntPoint& point) const
{
    return m_rootViewToPluginTransform.mapPoint(point);
}

} // namespace WebKit

#endif // ENABLE(LEGACY_PDFKIT_PLUGIN) || ENABLE(UNIFIED_PDF)
