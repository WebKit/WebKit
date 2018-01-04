/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
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
#import "CachedResourceLoader.h"
#import "ColorMac.h"
#import "DocumentFragment.h"
#import "DocumentLoader.h"
#import "Editing.h"
#import "EditingStyle.h"
#import "EditorClient.h"
#import "FontCascade.h"
#import "Frame.h"
#import "FrameLoader.h"
#import "FrameSelection.h"
#import "HTMLAttachmentElement.h"
#import "HTMLConverter.h"
#import "HTMLImageElement.h"
#import "HTMLSpanElement.h"
#import "LegacyWebArchive.h"
#import "Pasteboard.h"
#import "RenderElement.h"
#import "RenderStyle.h"
#import "Text.h"
#import "WebContentReader.h"
#import "WebCoreNSURLExtras.h"
#import "markup.h"
#import <pal/spi/cocoa/NSAttributedStringSPI.h>
#import <pal/spi/cocoa/NSKeyedArchiverSPI.h>
#import <wtf/BlockObjCExceptions.h>

namespace WebCore {

void Editor::getTextDecorationAttributesRespectingTypingStyle(const RenderStyle& style, NSMutableDictionary* result) const
{
    RefPtr<EditingStyle> typingStyle = m_frame.selection().typingStyle();
    if (typingStyle && typingStyle->style()) {
        RefPtr<CSSValue> value = typingStyle->style()->getPropertyCSSValue(CSSPropertyWebkitTextDecorationsInEffect);
        if (value && value->isValueList()) {
            CSSValueList& valueList = downcast<CSSValueList>(*value);
            if (valueList.hasValue(CSSValuePool::singleton().createIdentifierValue(CSSValueLineThrough).ptr()))
                [result setObject:@(NSUnderlineStyleSingle) forKey:NSStrikethroughStyleAttributeName];
            if (valueList.hasValue(CSSValuePool::singleton().createIdentifierValue(CSSValueUnderline).ptr()))
                [result setObject:@(NSUnderlineStyleSingle) forKey:NSUnderlineStyleAttributeName];
        }
    } else {
        int decoration = style.textDecorationsInEffect();
        if (decoration & TextDecorationLineThrough)
            [result setObject:@(NSUnderlineStyleSingle) forKey:NSStrikethroughStyleAttributeName];
        if (decoration & TextDecorationUnderline)
            [result setObject:@(NSUnderlineStyleSingle) forKey:NSUnderlineStyleAttributeName];
    }
}

RetainPtr<NSDictionary> Editor::fontAttributesForSelectionStart() const
{
    Node* nodeToRemove;
    auto* style = styleForSelectionStart(&m_frame, nodeToRemove);
    if (!style)
        return nil;

    RetainPtr<NSMutableDictionary> attributes = adoptNS([[NSMutableDictionary alloc] init]);

    if (auto ctFont = style->fontCascade().primaryFont().getCTFont())
        [attributes setObject:(id)ctFont forKey:NSFontAttributeName];

    // FIXME: Why would we not want to retrieve these attributes on iOS?
#if PLATFORM(MAC)
    if (style->visitedDependentColor(CSSPropertyBackgroundColor).isVisible())
        [attributes setObject:nsColor(style->visitedDependentColor(CSSPropertyBackgroundColor)) forKey:NSBackgroundColorAttributeName];

    if (style->visitedDependentColor(CSSPropertyColor).isValid() && !Color::isBlackColor(style->visitedDependentColor(CSSPropertyColor)))
        [attributes setObject:nsColor(style->visitedDependentColor(CSSPropertyColor)) forKey:NSForegroundColorAttributeName];

    const ShadowData* shadowData = style->textShadow();
    if (shadowData) {
        RetainPtr<NSShadow> platformShadow = adoptNS([[NSShadow alloc] init]);
        [platformShadow setShadowOffset:NSMakeSize(shadowData->x(), shadowData->y())];
        [platformShadow setShadowBlurRadius:shadowData->radius()];
        [platformShadow setShadowColor:nsColor(shadowData->color())];
        [attributes setObject:platformShadow.get() forKey:NSShadowAttributeName];
    }

    int superscriptInt = 0;
    switch (style->verticalAlign()) {
    case BASELINE:
    case BOTTOM:
    case BASELINE_MIDDLE:
    case LENGTH:
    case MIDDLE:
    case TEXT_BOTTOM:
    case TEXT_TOP:
    case TOP:
        break;
    case SUB:
        superscriptInt = -1;
        break;
    case SUPER:
        superscriptInt = 1;
        break;
    }
    if (superscriptInt)
        [attributes setObject:@(superscriptInt) forKey:NSSuperscriptAttributeName];
#endif

    getTextDecorationAttributesRespectingTypingStyle(*style, attributes.get());

    if (nodeToRemove)
        nodeToRemove->remove();

    return attributes;
}

static RefPtr<SharedBuffer> archivedDataForAttributedString(NSAttributedString *attributedString)
{
    if (!attributedString.length)
        return nullptr;

    return SharedBuffer::create(securelyArchivedDataWithRootObject(attributedString));
}

String Editor::selectionInHTMLFormat()
{
    if (auto range = selectedRange())
        return createMarkup(*range, nullptr, AnnotateForInterchange, false, ResolveNonLocalURLs);
    return { };
}

#if ENABLE(ATTACHMENT_ELEMENT)

void Editor::getPasteboardTypesAndDataForAttachment(HTMLAttachmentElement& attachment, Vector<String>& outTypes, Vector<RefPtr<SharedBuffer>>& outData)
{
    auto attachmentRange = Range::create(attachment.document(), { &attachment, Position::PositionIsBeforeAnchor }, { &attachment, Position::PositionIsAfterAnchor });
    client()->getClientPasteboardDataForRange(attachmentRange.ptr(), outTypes, outData);
    if (auto archive = LegacyWebArchive::create(attachmentRange.ptr())) {
        outTypes.append(WebArchivePboardType);
        outData.append(SharedBuffer::create(archive->rawDataRepresentation().get()));
    }
}

#endif

void Editor::writeSelectionToPasteboard(Pasteboard& pasteboard)
{
    NSAttributedString *attributedString = attributedStringFromRange(*selectedRange());

    PasteboardWebContent content;
    content.contentOrigin = m_frame.document()->originIdentifierForPasteboard();
    content.canSmartCopyOrDelete = canSmartCopyOrDelete();
    content.dataInWebArchiveFormat = selectionInWebArchiveFormat();
    content.dataInRTFDFormat = attributedString.containsAttachments ? dataInRTFDFormat(attributedString) : nullptr;
    content.dataInRTFFormat = dataInRTFFormat(attributedString);
    content.dataInAttributedStringFormat = archivedDataForAttributedString(attributedString);
    content.dataInHTMLFormat = selectionInHTMLFormat();
    content.dataInStringFormat = stringSelectionForPasteboardWithImageAltText();
    client()->getClientPasteboardDataForRange(selectedRange().get(), content.clientTypes, content.clientData);

    pasteboard.write(content);
}

void Editor::writeSelection(PasteboardWriterData& pasteboardWriterData)
{
    NSAttributedString *attributedString = attributedStringFromRange(*selectedRange());

    PasteboardWriterData::WebContent webContent;
    webContent.contentOrigin = m_frame.document()->originIdentifierForPasteboard();
    webContent.canSmartCopyOrDelete = canSmartCopyOrDelete();
    webContent.dataInWebArchiveFormat = selectionInWebArchiveFormat();
    webContent.dataInRTFDFormat = attributedString.containsAttachments ? dataInRTFDFormat(attributedString) : nullptr;
    webContent.dataInRTFFormat = dataInRTFFormat(attributedString);
    webContent.dataInAttributedStringFormat = archivedDataForAttributedString(attributedString);
    webContent.dataInHTMLFormat = selectionInHTMLFormat();
    webContent.dataInStringFormat = stringSelectionForPasteboardWithImageAltText();
    client()->getClientPasteboardDataForRange(selectedRange().get(), webContent.clientTypes, webContent.clientData);

    pasteboardWriterData.setWebContent(WTFMove(webContent));
}

RefPtr<SharedBuffer> Editor::selectionInWebArchiveFormat()
{
    RefPtr<LegacyWebArchive> archive = LegacyWebArchive::createFromSelection(&m_frame);
    if (!archive)
        return nullptr;
    return SharedBuffer::create(archive->rawDataRepresentation().get());
}

// FIXME: Makes no sense that selectedTextForDataTransfer always includes alt text, but stringSelectionForPasteboard does not.
// This was left in a bad state when selectedTextForDataTransfer was added. Need to look over clients and fix this.
String Editor::stringSelectionForPasteboard()
{
    if (!canCopy())
        return emptyString();
    String text = selectedText();
    text.replace(noBreakSpace, ' ');
    return text;
}

String Editor::stringSelectionForPasteboardWithImageAltText()
{
    if (!canCopy())
        return emptyString();
    String text = selectedTextForDataTransfer();
    text.replace(noBreakSpace, ' ');
    return text;
}

void Editor::replaceSelectionWithAttributedString(NSAttributedString *attributedString, MailBlockquoteHandling mailBlockquoteHandling)
{
    if (m_frame.selection().isNone())
        return;

    if (m_frame.selection().selection().isContentRichlyEditable()) {
        if (auto fragment = createFragmentAndAddResources(m_frame, attributedString)) {
            if (shouldInsertFragment(*fragment, selectedRange().get(), EditorInsertAction::Pasted))
                pasteAsFragment(fragment.releaseNonNull(), false, false, mailBlockquoteHandling);
        }
    } else {
        String text = attributedString.string;
        if (shouldInsertText(text, selectedRange().get(), EditorInsertAction::Pasted))
            pasteAsPlainText(text, false);
    }
}

String Editor::userVisibleString(const URL& url)
{
    return WebCore::userVisibleString(url);
}

RefPtr<SharedBuffer> Editor::dataInRTFDFormat(NSAttributedString *string)
{
    NSUInteger length = string.length;
    if (!length)
        return nullptr;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return SharedBuffer::create([string RTFDFromRange:NSMakeRange(0, length) documentAttributes:@{ }]);
    END_BLOCK_OBJC_EXCEPTIONS;

    return nullptr;
}

RefPtr<SharedBuffer> Editor::dataInRTFFormat(NSAttributedString *string)
{
    NSUInteger length = string.length;
    if (!length)
        return nullptr;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return SharedBuffer::create([string RTFFromRange:NSMakeRange(0, length) documentAttributes:@{ }]);
    END_BLOCK_OBJC_EXCEPTIONS;

    return nullptr;
}

}
