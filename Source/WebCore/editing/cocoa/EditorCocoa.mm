/*
 * Copyright (C) 2006-2016 Apple Inc. All rights reserved.
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

#import "config.h"
#import "Editor.h"

#import "ArchiveResource.h"
#import "CSSValueList.h"
#import "CSSValuePool.h"
#import "DocumentFragment.h"
#import "DocumentLoader.h"
#import "EditingStyle.h"
#import "EditorClient.h"
#import "Frame.h"
#import "FrameSelection.h"
#import "HTMLConverter.h"
#import "HTMLImageElement.h"
#import "HTMLSpanElement.h"
#import "LegacyWebArchive.h"
#import "NSAttributedStringSPI.h"
#import "Pasteboard.h"
#import "RenderElement.h"
#import "RenderStyle.h"
#import "SoftLinking.h"
#import "Text.h"
#import "htmlediting.h"
#import <wtf/BlockObjCExceptions.h>

#if PLATFORM(IOS)
SOFT_LINK_PRIVATE_FRAMEWORK(WebKitLegacy)
#endif

#if PLATFORM(MAC)
SOFT_LINK_FRAMEWORK_IN_UMBRELLA(WebKit, WebKitLegacy)
#endif

SOFT_LINK(WebKitLegacy, _WebCreateFragment, void, (WebCore::Document& document, NSAttributedString *string, WebCore::FragmentAndResources& result), (document, string, result))

namespace WebCore {

void Editor::getTextDecorationAttributesRespectingTypingStyle(const RenderStyle& style, NSMutableDictionary* result) const
{
    RefPtr<EditingStyle> typingStyle = m_frame.selection().typingStyle();
    if (typingStyle && typingStyle->style()) {
        RefPtr<CSSValue> value = typingStyle->style()->getPropertyCSSValue(CSSPropertyWebkitTextDecorationsInEffect);
        if (value && value->isValueList()) {
            CSSValueList& valueList = downcast<CSSValueList>(*value);
            if (valueList.hasValue(CSSValuePool::singleton().createIdentifierValue(CSSValueLineThrough).ptr()))
                [result setObject:[NSNumber numberWithInt:NSUnderlineStyleSingle] forKey:NSStrikethroughStyleAttributeName];
            if (valueList.hasValue(CSSValuePool::singleton().createIdentifierValue(CSSValueUnderline).ptr()))
                [result setObject:[NSNumber numberWithInt:NSUnderlineStyleSingle] forKey:NSUnderlineStyleAttributeName];
        }
    } else {
        int decoration = style.textDecorationsInEffect();
        if (decoration & TextDecorationLineThrough)
            [result setObject:[NSNumber numberWithInt:NSUnderlineStyleSingle] forKey:NSStrikethroughStyleAttributeName];
        if (decoration & TextDecorationUnderline)
            [result setObject:[NSNumber numberWithInt:NSUnderlineStyleSingle] forKey:NSUnderlineStyleAttributeName];
    }
}

FragmentAndResources Editor::createFragment(NSAttributedString *string)
{
    // FIXME: The algorithm to convert an attributed string into HTML should be implemented here in WebCore.
    // For now, though, we call into WebKitLegacy, which in turn calls into AppKit/TextKit.
    FragmentAndResources result;
    _WebCreateFragment(*m_frame.document(), string, result);
    return result;
}

void Editor::writeSelectionToPasteboard(Pasteboard& pasteboard)
{
    NSAttributedString *attributedString = attributedStringFromRange(*selectedRange());

    PasteboardWebContent content;
    content.canSmartCopyOrDelete = canSmartCopyOrDelete();
    content.dataInWebArchiveFormat = selectionInWebArchiveFormat();
    content.dataInRTFDFormat = attributedString.containsAttachments ? dataInRTFDFormat(attributedString) : nullptr;
    content.dataInRTFFormat = dataInRTFFormat(attributedString);
    // FIXME: Why don't we want this on iOS?
#if PLATFORM(MAC)
    content.dataInHTMLFormat = selectionInHTMLFormat();
#endif
    content.dataInStringFormat = stringSelectionForPasteboardWithImageAltText();
    client()->getClientPasteboardDataForRange(selectedRange().get(), content.clientTypes, content.clientData);

    pasteboard.write(content);
}

RefPtr<SharedBuffer> Editor::selectionInWebArchiveFormat()
{
    RefPtr<LegacyWebArchive> archive = LegacyWebArchive::createFromSelection(&m_frame);
    if (!archive)
        return nullptr;
    return SharedBuffer::wrapCFData(archive->rawDataRepresentation().get());
}

void Editor::replaceSelectionWithAttributedString(NSAttributedString *attributedString, MailBlockquoteHandling mailBlockquoteHandling)
{
    if (m_frame.selection().isNone())
        return;

    if (m_frame.selection().selection().isContentRichlyEditable()) {
        RefPtr<DocumentFragment> fragment = createFragmentAndAddResources(attributedString);
        if (fragment && shouldInsertFragment(fragment, selectedRange(), EditorInsertAction::Pasted))
            pasteAsFragment(fragment.releaseNonNull(), false, false, mailBlockquoteHandling);
    } else {
        String text = attributedString.string;
        if (shouldInsertText(text, selectedRange().get(), EditorInsertAction::Pasted))
            pasteAsPlainText(text, false);
    }
}

RefPtr<DocumentFragment> Editor::createFragmentForImageResourceAndAddResource(RefPtr<ArchiveResource>&& resource)
{
    if (!resource)
        return nullptr;

    // FIXME: Why is this different?
#if PLATFORM(MAC)
    String resourceURL = resource->url().string();
#else
    NSURL *URL = resource->url();
    String resourceURL = URL.isFileURL ? URL.absoluteString : resource->url();
#endif

    if (DocumentLoader* loader = m_frame.loader().documentLoader())
        loader->addArchiveResource(resource.releaseNonNull());

    auto imageElement = HTMLImageElement::create(*m_frame.document());
    imageElement->setAttributeWithoutSynchronization(HTMLNames::srcAttr, resourceURL);

    auto fragment = m_frame.document()->createDocumentFragment();
    fragment->appendChild(imageElement);
    
    return WTFMove(fragment);
}

RefPtr<SharedBuffer> Editor::dataInRTFDFormat(NSAttributedString *string)
{
    NSUInteger length = string.length;
    if (!length)
        return nullptr;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return SharedBuffer::wrapNSData([string RTFDFromRange:NSMakeRange(0, length) documentAttributes:@{ }]);
    END_BLOCK_OBJC_EXCEPTIONS;

    return nullptr;
}

RefPtr<SharedBuffer> Editor::dataInRTFFormat(NSAttributedString *string)
{
    NSUInteger length = string.length;
    if (!length)
        return nullptr;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return SharedBuffer::wrapNSData([string RTFFromRange:NSMakeRange(0, length) documentAttributes:@{ }]);
    END_BLOCK_OBJC_EXCEPTIONS;

    return nullptr;
}


}
