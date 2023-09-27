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
#include "UnifiedPDFPlugin.h"

#if ENABLE(UNIFIED_PDF)


namespace WebKit {
using namespace WebCore;

Ref<UnifiedPDFPlugin> UnifiedPDFPlugin::create(WebCore::HTMLPlugInElement& pluginElement)
{
    return adoptRef(*new UnifiedPDFPlugin(pluginElement));
}

UnifiedPDFPlugin::UnifiedPDFPlugin(HTMLPlugInElement& element)
    : PDFPluginBase(element)
{
}

void UnifiedPDFPlugin::teardown()
{

}

void UnifiedPDFPlugin::createPDFDocument()
{

}

void UnifiedPDFPlugin::installPDFDocument()
{

}

CGFloat UnifiedPDFPlugin::scaleFactor() const
{
    return 1;
}

RetainPtr<PDFDocument> UnifiedPDFPlugin::pdfDocumentForPrinting() const
{
    return nil;
}

WebCore::FloatSize UnifiedPDFPlugin::pdfDocumentSizeForPrinting() const
{
    return { };
}

RefPtr<FragmentedSharedBuffer> UnifiedPDFPlugin::liveResourceData() const
{
    return nullptr;
}

bool UnifiedPDFPlugin::handleMouseEvent(const WebMouseEvent&)
{
    return false;
}

bool UnifiedPDFPlugin::handleWheelEvent(const WebWheelEvent&)
{
    return false;
}

bool UnifiedPDFPlugin::handleMouseEnterEvent(const WebMouseEvent&)
{
    return false;
}

bool UnifiedPDFPlugin::handleMouseLeaveEvent(const WebMouseEvent&)
{
    return false;
}

bool UnifiedPDFPlugin::handleContextMenuEvent(const WebMouseEvent&)
{
    return false;
}

bool UnifiedPDFPlugin::handleKeyboardEvent(const WebKeyboardEvent&)
{
    return false;
}

bool UnifiedPDFPlugin::handleEditingCommand(StringView commandName)
{
    return false;
}

bool UnifiedPDFPlugin::isEditingCommandEnabled(StringView commandName)
{
    return false;
}

String UnifiedPDFPlugin::getSelectionString() const
{
    return emptyString();
}

bool UnifiedPDFPlugin::existingSelectionContainsPoint(const WebCore::FloatPoint&) const
{
    return false;
}

WebCore::FloatRect UnifiedPDFPlugin::rectForSelectionInRootView(PDFSelection *) const
{
    return { };
}

unsigned UnifiedPDFPlugin::countFindMatches(const String& target, WebCore::FindOptions, unsigned maxMatchCount)
{
    return 0;
}

bool UnifiedPDFPlugin::findString(const String& target, WebCore::FindOptions, unsigned maxMatchCount)
{
    return false;
}

bool UnifiedPDFPlugin::performDictionaryLookupAtLocation(const WebCore::FloatPoint&)
{
    return false;
}

std::tuple<String, PDFSelection *, NSDictionary *> UnifiedPDFPlugin::lookupTextAtLocation(const WebCore::FloatPoint&, WebHitTestResultData&) const
{
    return { };
}

RefPtr<ShareableBitmap> UnifiedPDFPlugin::snapshot()
{
    return nullptr;
}

id UnifiedPDFPlugin::accessibilityHitTest(const WebCore::IntPoint&) const
{
    return nil;
}

id UnifiedPDFPlugin::accessibilityObject() const
{
    return nil;
}

id UnifiedPDFPlugin::accessibilityAssociatedPluginParentForElement(WebCore::Element*) const
{
    return nil;
}


} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
