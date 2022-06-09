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
#import "Color.h"
#import "ColorCocoa.h"
#import "ColorSerialization.h"
#import "DataTransfer.h"
#import "DocumentFragment.h"
#import "Editing.h"
#import "EditorClient.h"
#import "FontAttributes.h"
#import "FontShadow.h"
#import "Frame.h"
#import "FrameView.h"
#import "HTMLConverter.h"
#import "HTMLElement.h"
#import "HTMLNames.h"
#import "LegacyNSPasteboardTypes.h"
#import "LegacyWebArchive.h"
#import "PagePasteboardContext.h"
#import "Pasteboard.h"
#import "PasteboardStrategy.h"
#import "PlatformStrategies.h"
#import "RenderBlock.h"
#import "RenderImage.h"
#import "RuntimeApplicationChecks.h"
#import "RuntimeEnabledFeatures.h"
#import "SharedBuffer.h"
#import "StyleProperties.h"
#import "WebContentReader.h"
#import "WebNSAttributedStringExtras.h"
#import "markup.h"
#import <AppKit/AppKit.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/NSURLExtras.h>

namespace WebCore {

void Editor::showFontPanel()
{
    auto* client = this->client();
    if (!client || !client->canShowFontPanel())
        return;

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
    auto range = selectedRange();

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // FIXME: How can this hard-coded pasteboard name be right, given that the passed-in pasteboard has a name?
    client()->setInsertionPasteboard(NSGeneralPboard);
    ALLOW_DEPRECATED_DECLARATIONS_END

    bool chosePlainText;
    RefPtr<DocumentFragment> fragment = webContentFromPasteboard(*pasteboard, *range, options.contains(PasteOption::AllowPlainText), chosePlainText);

    if (fragment && options.contains(PasteOption::AsQuotation))
        quoteFragmentForPasting(*fragment);

    if (fragment && shouldInsertFragment(*fragment, range, EditorInsertAction::Pasted))
        pasteAsFragment(fragment.releaseNonNull(), canSmartReplaceWithPasteboard(*pasteboard), false, options.contains(PasteOption::IgnoreMailBlockquote) ? MailBlockquoteHandling::IgnoreBlockquote : MailBlockquoteHandling::RespectBlockquote );

    client()->setInsertionPasteboard(String());
}

void Editor::platformCopyFont()
{
    Pasteboard pasteboard(PagePasteboardContext::create(m_document.pageID()), NSPasteboardNameFont);

    auto fontSampleString = adoptNS([[NSAttributedString alloc] initWithString:@"x" attributes:fontAttributesAtSelectionStart().createDictionary().get()]);
    auto fontData = RetainPtr([fontSampleString RTFFromRange:NSMakeRange(0, [fontSampleString length]) documentAttributes:@{ }]);

    PasteboardBuffer pasteboardBuffer;
    pasteboardBuffer.contentOrigin = m_document.originIdentifierForPasteboard();
    pasteboardBuffer.type = legacyFontPasteboardType();
    pasteboardBuffer.data = SharedBuffer::create(fontData.get());
    pasteboard.write(pasteboardBuffer);
}

void Editor::platformPasteFont()
{
    Pasteboard pasteboard(PagePasteboardContext::create(m_document.pageID()), NSPasteboardNameFont);

    client()->setInsertionPasteboard(pasteboard.name());

    RetainPtr<NSData> fontData;
    if (auto buffer = pasteboard.readBuffer(std::nullopt, legacyFontPasteboardType()))
        fontData = buffer->createNSData();
    auto fontSampleString = adoptNS([[NSAttributedString alloc] initWithRTF:fontData.get() documentAttributes:nil]);
    auto fontAttributes = RetainPtr([fontSampleString fontAttributesInRange:NSMakeRange(0, 1)]);

    auto style = MutableStyleProperties::create();

    Color backgroundColor;
    if (NSColor *nsBackgroundColor = dynamic_objc_cast<NSColor>([fontAttributes objectForKey:NSBackgroundColorAttributeName]))
        backgroundColor = colorFromCocoaColor(nsBackgroundColor);
    if (!backgroundColor.isValid())
        backgroundColor = Color::transparentBlack;
    style->setProperty(CSSPropertyBackgroundColor, CSSPrimitiveValue::create(backgroundColor));

    if (NSFont *font = dynamic_objc_cast<NSFont>([fontAttributes objectForKey:NSFontAttributeName])) {
        // FIXME: Need more sophisticated escaping code if we want to handle family names
        // with characters like single quote or backslash in their names.
        style->setProperty(CSSPropertyFontFamily, [NSString stringWithFormat:@"'%@'", [font familyName]]);
        style->setProperty(CSSPropertyFontSize, CSSPrimitiveValue::create([font pointSize], CSSUnitType::CSS_PX));
        // FIXME: Map to the entire range of CSS weight values.
        style->setProperty(CSSPropertyFontWeight, ([NSFontManager.sharedFontManager weightOfFont:font] >= 7) ? CSSValueBold : CSSValueNormal);
        style->setProperty(CSSPropertyFontStyle, ([NSFontManager.sharedFontManager traitsOfFont:font] & NSItalicFontMask) ? CSSValueItalic : CSSValueNormal);
    } else {
        style->setProperty(CSSPropertyFontFamily, "Helvetica"_s);
        style->setProperty(CSSPropertyFontSize, CSSPrimitiveValue::create(12, CSSUnitType::CSS_PX));
        style->setProperty(CSSPropertyFontWeight, CSSValueNormal);
        style->setProperty(CSSPropertyFontStyle, CSSValueNormal);
    }

    Color foregroundColor;
    if (NSColor *nsForegroundColor = dynamic_objc_cast<NSColor>([fontAttributes objectForKey:NSForegroundColorAttributeName])) {
        foregroundColor = colorFromCocoaColor(nsForegroundColor);
        if (!foregroundColor.isValid())
            foregroundColor = Color::transparentBlack;
    } else
        foregroundColor = Color::black;
    style->setProperty(CSSPropertyColor, CSSPrimitiveValue::create(foregroundColor));

    FontShadow fontShadow;
    if (NSShadow *nsFontShadow = dynamic_objc_cast<NSShadow>([fontAttributes objectForKey:NSShadowAttributeName]))
        fontShadow = fontShadowFromNSShadow(nsFontShadow);
    style->setProperty(CSSPropertyTextShadow, serializationForCSS(fontShadow));

    auto superscriptStyle = [[fontAttributes objectForKey:NSSuperscriptAttributeName] intValue];
    style->setProperty(CSSPropertyVerticalAlign, (superscriptStyle > 0) ? CSSValueSuper : ((superscriptStyle < 0) ? CSSValueSub : CSSValueBaseline));

    // FIXME: Underline wins here if we have both (see bug 3790443).
    auto underlineStyle = [[fontAttributes objectForKey:NSUnderlineStyleAttributeName] intValue];
    auto strikethroughStyle = [[fontAttributes objectForKey:NSStrikethroughStyleAttributeName] intValue];
    style->setProperty(CSSPropertyWebkitTextDecorationsInEffect, (underlineStyle != NSUnderlineStyleNone) ? CSSValueUnderline : ((strikethroughStyle != NSUnderlineStyleNone) ? CSSValueLineThrough : CSSValueNone));

    applyStyleToSelection(style.ptr(), EditAction::PasteFont);

    client()->setInsertionPasteboard(String());
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

    if (node->document() != m_document)
        return;

    Ref<Document> protector(m_document);

    auto range = makeRangeSelectingNodeContents(*node);
    m_document.selection().setSelection(VisibleSelection(range), FrameSelection::DoNotSetFocus);

    Pasteboard pasteboard(PagePasteboardContext::create(m_document.pageID()), pasteboardName);

    if (!m_document.selection().selection().isContentRichlyEditable()) {
        pasteAsPlainTextWithPasteboard(pasteboard);
        return;
    }

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // FIXME: How can this hard-coded pasteboard name be right, given that the passed-in pasteboard has a name?
    client()->setInsertionPasteboard(NSGeneralPboard);
    ALLOW_DEPRECATED_DECLARATIONS_END

    bool chosePlainText;
    if (auto fragment = webContentFromPasteboard(pasteboard, range, true, chosePlainText)) {
        maybeCopyNodeAttributesToFragment(*node, *fragment);
        if (shouldInsertFragment(*fragment, range, EditorInsertAction::Pasted))
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
    // FIXME: The interface to this function is awkward. We'd probably be better off with three separate functions. As of this writing, this is only used in WebKit2 to implement the method -[WKView writeSelectionToPasteboard:types:], which is only used to support OS X services.

    // FIXME: Does this function really need to use adjustedSelectionRange()? Because writeSelectionToPasteboard() just uses selectedRange(). This is the only function in WebKit that uses adjustedSelectionRange.
    if (!canCopy())
        return nullptr;

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (pasteboardType == WebArchivePboardType || pasteboardType == String(kUTTypeWebArchive))
        return selectionInWebArchiveFormat();
ALLOW_DEPRECATED_DECLARATIONS_END

    if (pasteboardType == String(legacyRTFDPasteboardType()))
        return dataInRTFDFormat(attributedString(*adjustedSelectionRange()).string.get());

    if (pasteboardType == String(legacyRTFPasteboardType())) {
        auto string = attributedString(*adjustedSelectionRange()).string;
        // FIXME: Why is this stripping needed here, but not in writeSelectionToPasteboard?
        if ([string containsAttachments])
            string = attributedStringByStrippingAttachmentCharacters(string.get());
        return dataInRTFFormat(string.get());
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
    if (!hasComposition() || ignoreSelectionChanges() || m_document.selection().isNone() || !m_document.hasLivingRenderTree())
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

    if (!pasteboard.isStatic())
        pasteboardImage.dataInWebArchiveFormat = imageInWebArchiveFormat(imageElement);

    if (auto imageRange = makeRangeSelectingNode(imageElement)) {
        auto serializeComposedTree = m_document.settings().selectionAcrossShadowBoundariesEnabled() ? SerializeComposedTree::Yes : SerializeComposedTree::No;
        pasteboardImage.dataInHTMLFormat = serializePreservingVisualAppearance(VisibleSelection { *imageRange }, ResolveURLs::YesExcludingLocalFileURLsForPrivacy, serializeComposedTree);
    }

    pasteboardImage.url.url = url;
    pasteboardImage.url.title = title;
    pasteboardImage.url.userVisibleForm = WTF::userVisibleString(pasteboardImage.url.url);
    if (auto* buffer = cachedImage->resourceBuffer())
        pasteboardImage.resourceData = buffer->makeContiguous();
    pasteboardImage.resourceMIMEType = cachedImage->response().mimeType();

    pasteboard.write(pasteboardImage);
}

} // namespace WebCore

#endif // PLATFORM(MAC)
