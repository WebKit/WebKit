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
#import "ElementInlines.h"
#import "FontAttributes.h"
#import "FontCascade.h"
#import "Frame.h"
#import "FrameLoader.h"
#import "FrameSelection.h"
#import "HTMLAttachmentElement.h"
#import "HTMLConverter.h"
#import "HTMLImageElement.h"
#import "HTMLSpanElement.h"
#import "ImageOverlay.h"
#import "LegacyNSPasteboardTypes.h"
#import "LegacyWebArchive.h"
#import "Page.h"
#import "PagePasteboardContext.h"
#import "Pasteboard.h"
#import "PasteboardStrategy.h"
#import "PlatformStrategies.h"
#import "RenderElement.h"
#import "RenderStyle.h"
#import "Settings.h"
#import "SystemSoundManager.h"
#import "Text.h"
#import "UTIUtilities.h"
#import "WebContentReader.h"
#import "markup.h"
#import <pal/spi/cocoa/NSAttributedStringSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/cocoa/NSURLExtras.h>

namespace WebCore {

static RefPtr<SharedBuffer> archivedDataForAttributedString(NSAttributedString *attributedString)
{
    if (!attributedString.length)
        return nullptr;

    return SharedBuffer::create([NSKeyedArchiver archivedDataWithRootObject:attributedString requiringSecureCoding:YES error:nullptr]);
}

String Editor::selectionInHTMLFormat()
{
    if (ImageOverlay::isInsideOverlay(m_document.selection().selection()))
        return { };
    return serializePreservingVisualAppearance(m_document.selection().selection(), ResolveURLs::YesExcludingURLsForPrivacy, SerializeComposedTree::Yes, IgnoreUserSelectNone::Yes);
}

#if ENABLE(ATTACHMENT_ELEMENT)

void Editor::getPasteboardTypesAndDataForAttachment(Element& element, Vector<String>& outTypes, Vector<RefPtr<SharedBuffer>>& outData)
{
    auto elementRange = makeRangeSelectingNode(element);
    client()->getClientPasteboardData(elementRange, outTypes, outData);

    outTypes.append(PasteboardCustomData::cocoaType());
    outData.append(PasteboardCustomData { element.document().originIdentifierForPasteboard(), { } }.createSharedBuffer());

    if (elementRange) {
        if (auto archive = LegacyWebArchive::create(*elementRange)) {
            if (auto data = archive->rawDataRepresentation()) {
                outTypes.append(WebArchivePboardType);
                outData.append(SharedBuffer::create(data.get()));
            }
        }
    }
}

#endif

static RetainPtr<NSAttributedString> selectionInImageOverlayAsAttributedString(const VisibleSelection& selection)
{
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    auto* page = selection.document()->page();
    if (!page)
        return nil;

    RefPtr hostElement = dynamicDowncast<HTMLElement>(selection.start().containerNode()->shadowHost());
    if (!hostElement) {
        ASSERT_NOT_REACHED();
        return nil;
    }

    auto cachedResult = page->cachedTextRecognitionResult(*hostElement);
    if (!cachedResult)
        return nil;

    auto characterRange = valueOrDefault(ImageOverlay::characterRange(selection));
    if (!characterRange.length)
        return nil;

    auto string = stringForRange(*cachedResult, characterRange);
    __block bool hasAnyAttributes = false;
    [string enumerateAttributesInRange:NSMakeRange(0, [string length]) options:0 usingBlock:^(NSDictionary *attributes, NSRange, BOOL *stop) {
        if (attributes.count) {
            hasAnyAttributes = true;
            *stop = YES;
        }
    }];

    if (!hasAnyAttributes)
        return nil;

    return string;
#else
    UNUSED_PARAM(selection);
    return nil;
#endif
}

static RetainPtr<NSAttributedString> selectionAsAttributedString(const Document& document)
{
    auto selection = document.selection().selection();
    if (ImageOverlay::isInsideOverlay(selection))
        return selectionInImageOverlayAsAttributedString(selection);
    auto range = selection.firstRange();
    return range ? attributedString(*range).string : adoptNS([[NSAttributedString alloc] init]);
}

void Editor::writeSelectionToPasteboard(Pasteboard& pasteboard)
{
    auto string = selectionAsAttributedString(m_document);

    PasteboardWebContent content;
    content.contentOrigin = m_document.originIdentifierForPasteboard();
    content.canSmartCopyOrDelete = canSmartCopyOrDelete();
    if (!pasteboard.isStatic()) {
        content.dataInWebArchiveFormat = selectionInWebArchiveFormat();
        content.dataInRTFDFormat = [string containsAttachments] ? dataInRTFDFormat(string.get()) : nullptr;
        content.dataInRTFFormat = dataInRTFFormat(string.get());
        content.dataInAttributedStringFormat = archivedDataForAttributedString(string.get());
        client()->getClientPasteboardData(selectedRange(), content.clientTypes, content.clientData);
    }
    content.dataInHTMLFormat = selectionInHTMLFormat();
    content.dataInStringFormat = stringSelectionForPasteboardWithImageAltText();

    pasteboard.write(content);
}

void Editor::writeSelection(PasteboardWriterData& pasteboardWriterData)
{
    auto string = selectionAsAttributedString(m_document);

    PasteboardWriterData::WebContent webContent;
    webContent.contentOrigin = m_document.originIdentifierForPasteboard();
    webContent.canSmartCopyOrDelete = canSmartCopyOrDelete();
    webContent.dataInWebArchiveFormat = selectionInWebArchiveFormat();
    webContent.dataInRTFDFormat = [string containsAttachments] ? dataInRTFDFormat(string.get()) : nullptr;
    webContent.dataInRTFFormat = dataInRTFFormat(string.get());
    webContent.dataInAttributedStringFormat = archivedDataForAttributedString(string.get());
    webContent.dataInHTMLFormat = selectionInHTMLFormat();
    webContent.dataInStringFormat = stringSelectionForPasteboardWithImageAltText();
    client()->getClientPasteboardData(selectedRange(), webContent.clientTypes, webContent.clientData);

    pasteboardWriterData.setWebContent(WTFMove(webContent));
}

RefPtr<SharedBuffer> Editor::selectionInWebArchiveFormat()
{
    if (ImageOverlay::isInsideOverlay(m_document.selection().selection()))
        return nullptr;
    auto archive = LegacyWebArchive::createFromSelection(m_document.frame());
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
    return makeStringByReplacingAll(selectedText(), noBreakSpace, ' ');
}

String Editor::stringSelectionForPasteboardWithImageAltText()
{
    if (!canCopy())
        return emptyString();
    return makeStringByReplacingAll(selectedTextForDataTransfer(), noBreakSpace, ' ');
}

void Editor::replaceSelectionWithAttributedString(NSAttributedString *attributedString, MailBlockquoteHandling mailBlockquoteHandling)
{
    if (m_document.selection().isNone())
        return;

    if (m_document.selection().selection().isContentRichlyEditable()) {
        if (auto fragment = createFragmentAndAddResources(*m_document.frame(), attributedString)) {
            if (shouldInsertFragment(*fragment, selectedRange(), EditorInsertAction::Pasted))
                pasteAsFragment(fragment.releaseNonNull(), false, false, mailBlockquoteHandling);
        }
    } else {
        String text = attributedString.string;
        if (shouldInsertText(text, selectedRange(), EditorInsertAction::Pasted))
            pasteAsPlainText(text, false);
    }
}

String Editor::userVisibleString(const URL& url)
{
    return WTF::userVisibleString(url);
}

RefPtr<SharedBuffer> Editor::dataInRTFDFormat(NSAttributedString *string)
{
    NSUInteger length = string.length;
    if (!length)
        return nullptr;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    return SharedBuffer::create([string RTFDFromRange:NSMakeRange(0, length) documentAttributes:@{ }]);
    END_BLOCK_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<SharedBuffer> Editor::dataInRTFFormat(NSAttributedString *string)
{
    NSUInteger length = string.length;
    if (!length)
        return nullptr;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    return SharedBuffer::create([string RTFFromRange:NSMakeRange(0, length) documentAttributes:@{ }]);
    END_BLOCK_OBJC_EXCEPTIONS

    return nullptr;
}

// FIXME: Should give this function a name that makes it clear it adds resources to the document loader as a side effect.
// Or refactor so it does not do that.
RefPtr<DocumentFragment> Editor::webContentFromPasteboard(Pasteboard& pasteboard, const SimpleRange& context, bool allowPlainText, bool& chosePlainText)
{
    WebContentReader reader(*m_document.frame(), context, allowPlainText);
    pasteboard.read(reader);
    chosePlainText = reader.madeFragmentFromPlainText;
    return WTFMove(reader.fragment);
}

void Editor::takeFindStringFromSelection()
{
    if (!canCopyExcludingStandaloneImages()) {
        SystemSoundManager::singleton().systemBeep();
        return;
    }

    auto stringFromSelection = m_document.frame()->displayStringModifiedByEncoding(selectedTextForDataTransfer());
#if PLATFORM(MAC)
    Vector<String> types;
    types.append(String(legacyStringPasteboardType()));
    auto context = PagePasteboardContext::create(m_document.pageID());
    platformStrategies()->pasteboardStrategy()->setTypes(types, NSPasteboardNameFind, context.get());
    platformStrategies()->pasteboardStrategy()->setStringForType(WTFMove(stringFromSelection), legacyStringPasteboardType(), NSPasteboardNameFind, context.get());
#else
    if (auto* client = this->client()) {
        // Since the find pasteboard doesn't exist on iOS, WebKit maintains its own notion of the latest find string,
        // which SPI clients may respect when presenting find-in-page UI.
        client->updateStringForFind(stringFromSelection);
    }
#endif
}

String Editor::platformContentTypeForBlobType(const String& type) const
{
    auto utiType = UTIFromMIMEType(type);
    if (!utiType.isEmpty())
        return utiType;
    return type;
}

void Editor::readSelectionFromPasteboard(const String& pasteboardName)
{
    Pasteboard pasteboard(PagePasteboardContext::create(m_document.pageID()), pasteboardName);
    if (m_document.selection().selection().isContentRichlyEditable())
        pasteWithPasteboard(&pasteboard, { PasteOption::AllowPlainText });
    else
        pasteAsPlainTextWithPasteboard(pasteboard);
}

static void maybeCopyNodeAttributesToFragment(const Node& node, DocumentFragment& fragment)
{
    // This is only supported for single-Node fragments.
    RefPtr firstChild = fragment.firstChild();
    if (!firstChild || firstChild != fragment.lastChild())
        return;

    // And only supported for HTML elements.
    if (!node.isHTMLElement() || !firstChild->isHTMLElement())
        return;

    // And only if the source Element and destination Element have the same HTML tag name.
    Ref oldElement = downcast<HTMLElement>(node);
    Ref newElement = downcast<HTMLElement>(*firstChild);
    if (oldElement->localName() != newElement->localName())
        return;

    for (auto& attribute : oldElement->attributesIterator()) {
        if (newElement->hasAttribute(attribute.name()))
            continue;
        newElement->setAttribute(attribute.name(), attribute.value());
    }
}

void Editor::replaceNodeFromPasteboard(Node& node, const String& pasteboardName, EditAction action)
{
    if (node.document() != m_document)
        return;

    auto range = makeRangeSelectingNode(node);
    if (!range)
        return;

    Ref protectedDocument = m_document;
    m_document.selection().setSelection({ *range }, FrameSelection::DoNotSetFocus);

    Pasteboard pasteboard(PagePasteboardContext::create(m_document.pageID()), pasteboardName);
    if (!m_document.selection().selection().isContentRichlyEditable()) {
        pasteAsPlainTextWithPasteboard(pasteboard);
        return;
    }

#if PLATFORM(MAC)
    // FIXME: How can this hard-coded pasteboard name be right, given that the passed-in pasteboard has a name?
    // FIXME: We can also remove `setInsertionPasteboard` altogether once Mail compose on macOS no longer uses WebKitLegacy,
    // since it's only implemented for WebKitLegacy on macOS, and the only known client is Mail compose.
    client()->setInsertionPasteboard(NSPasteboardNameGeneral);
#endif

    bool chosePlainText;
    if (auto fragment = webContentFromPasteboard(pasteboard, *range, true, chosePlainText)) {
        maybeCopyNodeAttributesToFragment(node, *fragment);
        if (shouldInsertFragment(*fragment, *range, EditorInsertAction::Pasted))
            pasteAsFragment(fragment.releaseNonNull(), false, false, MailBlockquoteHandling::IgnoreBlockquote, action);
    }

#if PLATFORM(MAC)
    client()->setInsertionPasteboard({ });
#endif
}

}
