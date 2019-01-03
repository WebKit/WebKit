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

#if PLATFORM(MAC)

#import "Blob.h"
#import "CSSPrimitiveValueMappings.h"
#import "CSSValuePool.h"
#import "DataTransfer.h"
#import "DocumentFragment.h"
#import "Editing.h"
#import "EditorClient.h"
#import "Frame.h"
#import "FrameView.h"
#import "HTMLConverter.h"
#import "HTMLElement.h"
#import "HTMLNames.h"
#import "LegacyNSPasteboardTypes.h"
#import "LegacyWebArchive.h"
#import "Pasteboard.h"
#import "PasteboardStrategy.h"
#import "PlatformStrategies.h"
#import "Range.h"
#import "RenderBlock.h"
#import "RenderImage.h"
#import "RuntimeApplicationChecks.h"
#import "RuntimeEnabledFeatures.h"
#import "StyleProperties.h"
#import "WebContentReader.h"
#import "WebNSAttributedStringExtras.h"
#import "markup.h"
#import <AppKit/AppKit.h>
#import <wtf/cocoa/NSURLExtras.h>

namespace WebCore {

using namespace HTMLNames;

void Editor::showFontPanel()
{
    [[NSFontManager sharedFontManager] orderFrontFontPanel:nil];
}

void Editor::showStylesPanel()
{
    [[NSFontManager sharedFontManager] orderFrontStylesPanel:nil];
}

void Editor::showColorPanel()
{
    [[NSApplication sharedApplication] orderFrontColorPanel:nil];
}

void Editor::pasteWithPasteboard(Pasteboard* pasteboard, OptionSet<PasteOption> options)
{
    RefPtr<Range> range = selectedRange();

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // FIXME: How can this hard-coded pasteboard name be right, given that the passed-in pasteboard has a name?
    client()->setInsertionPasteboard(NSGeneralPboard);
    ALLOW_DEPRECATED_DECLARATIONS_END

    bool chosePlainText;
    RefPtr<DocumentFragment> fragment = webContentFromPasteboard(*pasteboard, *range, options.contains(PasteOption::AllowPlainText), chosePlainText);

    if (fragment && options.contains(PasteOption::AsQuotation))
        quoteFragmentForPasting(*fragment);

    if (fragment && shouldInsertFragment(*fragment, range.get(), EditorInsertAction::Pasted))
        pasteAsFragment(fragment.releaseNonNull(), canSmartReplaceWithPasteboard(*pasteboard), false, options.contains(PasteOption::IgnoreMailBlockquote) ? MailBlockquoteHandling::IgnoreBlockquote : MailBlockquoteHandling::RespectBlockquote );

    client()->setInsertionPasteboard(String());
}

void Editor::readSelectionFromPasteboard(const String& pasteboardName)
{
    Pasteboard pasteboard(pasteboardName);
    if (m_frame.selection().selection().isContentRichlyEditable())
        pasteWithPasteboard(&pasteboard, { PasteOption::AllowPlainText });
    else
        pasteAsPlainTextWithPasteboard(pasteboard);
}

static void maybeCopyNodeAttributesToFragment(const Node& node, DocumentFragment& fragment)
{
    // This is only supported for single-Node fragments.
    Node* firstChild = fragment.firstChild();
    if (!firstChild || firstChild != fragment.lastChild())
        return;

    // And only supported for HTML elements.
    if (!node.isHTMLElement() || !firstChild->isHTMLElement())
        return;

    // And only if the source Element and destination Element have the same HTML tag name.
    const HTMLElement& oldElement = downcast<HTMLElement>(node);
    HTMLElement& newElement = downcast<HTMLElement>(*firstChild);
    if (oldElement.localName() != newElement.localName())
        return;

    for (const Attribute& attribute : oldElement.attributesIterator()) {
        if (newElement.hasAttribute(attribute.name()))
            continue;
        newElement.setAttribute(attribute.name(), attribute.value());
    }
}

void Editor::replaceNodeFromPasteboard(Node* node, const String& pasteboardName)
{
    ASSERT(node);

    if (&node->document() != m_frame.document())
        return;

    Ref<Frame> protector(m_frame);
    auto range = Range::create(node->document(), Position(node, Position::PositionIsBeforeAnchor), Position(node, Position::PositionIsAfterAnchor));
    m_frame.selection().setSelection(VisibleSelection(range.get()), FrameSelection::DoNotSetFocus);

    Pasteboard pasteboard(pasteboardName);

    if (!m_frame.selection().selection().isContentRichlyEditable()) {
        pasteAsPlainTextWithPasteboard(pasteboard);
        return;
    }

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // FIXME: How can this hard-coded pasteboard name be right, given that the passed-in pasteboard has a name?
    client()->setInsertionPasteboard(NSGeneralPboard);
    ALLOW_DEPRECATED_DECLARATIONS_END

    bool chosePlainText;
    if (RefPtr<DocumentFragment> fragment = webContentFromPasteboard(pasteboard, range.get(), true, chosePlainText)) {
        maybeCopyNodeAttributesToFragment(*node, *fragment);
        if (shouldInsertFragment(*fragment, range.ptr(), EditorInsertAction::Pasted))
            pasteAsFragment(fragment.releaseNonNull(), canSmartReplaceWithPasteboard(pasteboard), false, MailBlockquoteHandling::IgnoreBlockquote);
    }

    client()->setInsertionPasteboard(String());
}

RefPtr<SharedBuffer> Editor::imageInWebArchiveFormat(Element& imageElement)
{
    auto archive = LegacyWebArchive::create(imageElement);
    if (!archive)
        return nullptr;
    return SharedBuffer::create(archive->rawDataRepresentation().get());
}

RefPtr<SharedBuffer> Editor::dataSelectionForPasteboard(const String& pasteboardType)
{
    // FIXME: The interface to this function is awkward. We'd probably be better off with three separate functions.
    // As of this writing, this is only used in WebKit2 to implement the method -[WKView writeSelectionToPasteboard:types:],
    // which is only used to support OS X services.

    // FIXME: Does this function really need to use adjustedSelectionRange()? Because writeSelectionToPasteboard() just uses selectedRange().
    if (!canCopy())
        return nullptr;

    if (pasteboardType == WebArchivePboardType)
        return selectionInWebArchiveFormat();

    if (pasteboardType == String(legacyRTFDPasteboardType()))
       return dataInRTFDFormat(attributedStringFromRange(*adjustedSelectionRange()));

    if (pasteboardType == String(legacyRTFPasteboardType())) {
        NSAttributedString* attributedString = attributedStringFromRange(*adjustedSelectionRange());
        // FIXME: Why is this attachment character stripping needed here, but not needed in writeSelectionToPasteboard?
        if ([attributedString containsAttachments])
            attributedString = attributedStringByStrippingAttachmentCharacters(attributedString);
        return dataInRTFFormat(attributedString);
    }

    return nullptr;
}

static void getImage(Element& imageElement, RefPtr<Image>& image, CachedImage*& cachedImage)
{
    auto* renderer = imageElement.renderer();
    if (!is<RenderImage>(renderer))
        return;

    CachedImage* tentativeCachedImage = downcast<RenderImage>(*renderer).cachedImage();
    if (!tentativeCachedImage || tentativeCachedImage->errorOccurred())
        return;

    image = tentativeCachedImage->imageForRenderer(renderer);
    if (!image)
        return;

    cachedImage = tentativeCachedImage;
}

void Editor::selectionWillChange()
{
    if (!hasComposition() || ignoreSelectionChanges() || m_frame.selection().isNone())
        return;

    cancelComposition();
    client()->canceledComposition();
}

String Editor::plainTextFromPasteboard(const PasteboardPlainText& text)
{
    auto string = text.text;

    // FIXME: It's not clear this is 100% correct since we know -[NSURL URLWithString:] does not handle
    // all the same cases we handle well in the URL code for creating an NSURL.
    if (text.isURL)
        string = WTF::userVisibleString([NSURL URLWithString:string]);

    // FIXME: WTF should offer a non-Mac-specific way to convert string to precomposed form so we can do it for all platforms.
    return [(NSString *)string precomposedStringWithCanonicalMapping];
}

void Editor::writeImageToPasteboard(Pasteboard& pasteboard, Element& imageElement, const URL& url, const String& title)
{
    PasteboardImage pasteboardImage;

    CachedImage* cachedImage = nullptr;
    getImage(imageElement, pasteboardImage.image, cachedImage);
    if (!pasteboardImage.image)
        return;
    ASSERT(cachedImage);

    pasteboardImage.dataInWebArchiveFormat = imageInWebArchiveFormat(imageElement);
    pasteboardImage.url.url = url;
    pasteboardImage.url.title = title;
    pasteboardImage.url.userVisibleForm = WTF::userVisibleString(pasteboardImage.url.url);
    pasteboardImage.resourceData = cachedImage->resourceBuffer();
    pasteboardImage.resourceMIMEType = cachedImage->response().mimeType();

    pasteboard.write(pasteboardImage);
}

} // namespace WebCore

#endif // PLATFORM(MAC)
