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

#import "Blob.h"
#import "CSSPrimitiveValueMappings.h"
#import "CSSValuePool.h"
#import "DOMURL.h"
#import "DataTransfer.h"
#import "DocumentFragment.h"
#import "DocumentLoader.h"
#import "Editing.h"
#import "Editor.h"
#import "EditorClient.h"
#import "File.h"
#import "FontCascade.h"
#import "Frame.h"
#import "FrameLoaderClient.h"
#import "FrameView.h"
#import "HTMLAnchorElement.h"
#import "HTMLAttachmentElement.h"
#import "HTMLConverter.h"
#import "HTMLElement.h"
#import "HTMLImageElement.h"
#import "HTMLNames.h"
#import "LegacyWebArchive.h"
#import "MIMETypeRegistry.h"
#import "NodeTraversal.h"
#import "Page.h"
#import "Pasteboard.h"
#import "PasteboardStrategy.h"
#import "PlatformStrategies.h"
#import "Range.h"
#import "RenderBlock.h"
#import "RenderImage.h"
#import "RuntimeApplicationChecks.h"
#import "Settings.h"
#import "Sound.h"
#import "StyleProperties.h"
#import "Text.h"
#import "TypingCommand.h"
#import "UUID.h"
#import "WebNSAttributedStringExtras.h"
#import "markup.h"

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

void Editor::pasteWithPasteboard(Pasteboard* pasteboard, bool allowPlainText, MailBlockquoteHandling mailBlockquoteHandling)
{
    RefPtr<Range> range = selectedRange();

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    // FIXME: How can this hard-coded pasteboard name be right, given that the passed-in pasteboard has a name?
    client()->setInsertionPasteboard(NSGeneralPboard);
#pragma clang diagnostic pop

    bool chosePlainText;
    RefPtr<DocumentFragment> fragment = webContentFromPasteboard(*pasteboard, *range, allowPlainText, chosePlainText);

    if (fragment && shouldInsertFragment(fragment, range, EditorInsertAction::Pasted))
        pasteAsFragment(fragment.releaseNonNull(), canSmartReplaceWithPasteboard(*pasteboard), false, mailBlockquoteHandling);

    client()->setInsertionPasteboard(String());
}

bool Editor::canCopyExcludingStandaloneImages()
{
    const VisibleSelection& selection = m_frame.selection().selection();
    return selection.isRange() && !selection.isInPasswordField();
}

void Editor::takeFindStringFromSelection()
{
    if (!canCopyExcludingStandaloneImages()) {
        systemBeep();
        return;
    }

    Vector<String> types;
    types.append(String(NSStringPboardType));
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    platformStrategies()->pasteboardStrategy()->setTypes(types, NSFindPboard);
    platformStrategies()->pasteboardStrategy()->setStringForType(m_frame.displayStringModifiedByEncoding(selectedTextForDataTransfer()), NSStringPboardType, NSFindPboard);
#pragma clang diagnostic pop
}

void Editor::readSelectionFromPasteboard(const String& pasteboardName, MailBlockquoteHandling mailBlockquoteHandling)
{
    Pasteboard pasteboard(pasteboardName);
    if (m_frame.selection().selection().isContentRichlyEditable())
        pasteWithPasteboard(&pasteboard, true, mailBlockquoteHandling);
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
    RefPtr<Range> range = Range::create(node->document(), Position(node, Position::PositionIsBeforeAnchor), Position(node, Position::PositionIsAfterAnchor));
    m_frame.selection().setSelection(VisibleSelection(*range), FrameSelection::DoNotSetFocus);

    Pasteboard pasteboard(pasteboardName);

    if (!m_frame.selection().selection().isContentRichlyEditable()) {
        pasteAsPlainTextWithPasteboard(pasteboard);
        return;
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    // FIXME: How can this hard-coded pasteboard name be right, given that the passed-in pasteboard has a name?
    client()->setInsertionPasteboard(NSGeneralPboard);
#pragma clang diagnostic pop

    bool chosePlainText;
    if (RefPtr<DocumentFragment> fragment = webContentFromPasteboard(pasteboard, *range, true, chosePlainText)) {
        maybeCopyNodeAttributesToFragment(*node, *fragment);
        if (shouldInsertFragment(fragment, range, EditorInsertAction::Pasted))
            pasteAsFragment(fragment.releaseNonNull(), canSmartReplaceWithPasteboard(pasteboard), false, MailBlockquoteHandling::IgnoreBlockquote);
    }

    client()->setInsertionPasteboard(String());
}

String Editor::selectionInHTMLFormat()
{
    return createMarkup(*selectedRange(), nullptr, AnnotateForInterchange, false, ResolveNonLocalURLs);
}

RefPtr<SharedBuffer> Editor::imageInWebArchiveFormat(Element& imageElement)
{
    RefPtr<LegacyWebArchive> archive = LegacyWebArchive::create(imageElement);
    if (!archive)
        return nullptr;
    return SharedBuffer::wrapCFData(archive->rawDataRepresentation().get());
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

    if (pasteboardType == String(NSRTFDPboardType))
       return dataInRTFDFormat(attributedStringFromRange(*adjustedSelectionRange()));

    if (pasteboardType == String(NSRTFPboardType)) {
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

String Editor::userVisibleString(const URL& url)
{
    return client()->userVisibleString(url);
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
    String string = text.text;

    // FIXME: It's not clear this is 100% correct since we know -[NSURL URLWithString:] does not handle
    // all the same cases we handle well in the URL code for creating an NSURL.
    if (text.isURL)
        string = client()->userVisibleString([NSURL URLWithString:string]);

    // FIXME: WTF should offer a non-Mac-specific way to convert string to precomposed form so we can do it for all platforms.
    return [(NSString *)string precomposedStringWithCanonicalMapping];
}

void Editor::writeImageToPasteboard(Pasteboard& pasteboard, Element& imageElement, const URL& url, const String& title)
{
    PasteboardImage pasteboardImage;

    CachedImage* cachedImage;
    getImage(imageElement, pasteboardImage.image, cachedImage);
    if (!pasteboardImage.image)
        return;
    ASSERT(cachedImage);

    pasteboardImage.dataInWebArchiveFormat = imageInWebArchiveFormat(imageElement);
    pasteboardImage.url.url = url;
    pasteboardImage.url.title = title;
    pasteboardImage.url.userVisibleForm = client()->userVisibleString(pasteboardImage.url.url);
    pasteboardImage.resourceData = cachedImage->resourceBuffer();
    pasteboardImage.resourceMIMEType = cachedImage->response().mimeType();

    pasteboard.write(pasteboardImage);
}

class Editor::WebContentReader final : public PasteboardWebContentReader {
public:
    Frame& frame;
    Range& context;
    const bool allowPlainText;

    RefPtr<DocumentFragment> fragment;
    bool madeFragmentFromPlainText;

    WebContentReader(Frame& frame, Range& context, bool allowPlainText)
        : frame(frame)
        , context(context)
        , allowPlainText(allowPlainText)
        , madeFragmentFromPlainText(false)
    {
    }

private:
    bool readWebArchive(SharedBuffer*) override;
    bool readFilenames(const Vector<String>&) override;
    bool readHTML(const String&) override;
    bool readRTFD(SharedBuffer&) override;
    bool readRTF(SharedBuffer&) override;
    bool readImage(Ref<SharedBuffer>&&, const String& type) override;
    bool readURL(const URL&, const String& title) override;
    bool readPlainText(const String&) override;
};

bool Editor::WebContentReader::readWebArchive(SharedBuffer* buffer)
{
    if (frame.settings().preferMIMETypeForImages())
        return false;

    if (!frame.document())
        return false;

    if (!buffer)
        return false;

    auto archive = LegacyWebArchive::create(URL(), *buffer);
    if (!archive)
        return false;

    RefPtr<ArchiveResource> mainResource = archive->mainResource();
    if (!mainResource)
        return false;

    const String& type = mainResource->mimeType();

    if (frame.loader().client().canShowMIMETypeAsHTML(type)) {
        // FIXME: The code in createFragmentAndAddResources calls setDefersLoading(true). Don't we need that here?
        if (DocumentLoader* loader = frame.loader().documentLoader())
            loader->addAllArchiveResources(*archive);

        String markupString = String::fromUTF8(mainResource->data().data(), mainResource->data().size());
        fragment = createFragmentFromMarkup(*frame.document(), markupString, mainResource->url(), DisallowScriptingAndPluginContent);
        return true;
    }

    if (MIMETypeRegistry::isSupportedImageMIMEType(type)) {
        fragment = frame.editor().createFragmentForImageResourceAndAddResource(WTFMove(mainResource));
        return true;
    }

    return false;
}

bool Editor::WebContentReader::readFilenames(const Vector<String>& paths)
{
    if (paths.isEmpty())
        return false;

    if (!frame.document())
        return false;
    Document& document = *frame.document();

    fragment = document.createDocumentFragment();

    for (auto& text : paths) {
#if ENABLE(ATTACHMENT_ELEMENT)
        auto attachment = HTMLAttachmentElement::create(attachmentTag, document);
        attachment->setFile(File::create([[NSURL fileURLWithPath:text] path]).ptr());
        fragment->appendChild(attachment);
#else
        auto paragraph = createDefaultParagraphElement(document);
        paragraph->appendChild(document.createTextNode(frame.editor().client()->userVisibleString([NSURL fileURLWithPath:text])));
        fragment->appendChild(paragraph);
#endif
    }

    return true;
}

bool Editor::WebContentReader::readHTML(const String& string)
{
    String stringOmittingMicrosoftPrefix = string;

    // This code was added to make HTML paste from Microsoft Word on Mac work, back in 2004.
    // It's a simple-minded way to ignore the CF_HTML clipboard format, just skipping over the
    // description part and parsing the entire context plus fragment.
    if (string.startsWith("Version:")) {
        size_t location = string.findIgnoringCase("<html");
        if (location != notFound)
            stringOmittingMicrosoftPrefix = string.substring(location);
    }

    if (stringOmittingMicrosoftPrefix.isEmpty())
        return false;

    if (!frame.document())
        return false;
    Document& document = *frame.document();

    fragment = createFragmentFromMarkup(document, stringOmittingMicrosoftPrefix, emptyString(), DisallowScriptingAndPluginContent);
    return fragment;
}

bool Editor::WebContentReader::readRTFD(SharedBuffer& buffer)
{
    if (frame.settings().preferMIMETypeForImages())
        return false;

    fragment = frame.editor().createFragmentAndAddResources(adoptNS([[NSAttributedString alloc] initWithRTFD:buffer.createNSData().get() documentAttributes:nullptr]).get());
    return fragment;
}

bool Editor::WebContentReader::readRTF(SharedBuffer& buffer)
{
    if (frame.settings().preferMIMETypeForImages())
        return false;

    fragment = frame.editor().createFragmentAndAddResources(adoptNS([[NSAttributedString alloc] initWithRTF:buffer.createNSData().get() documentAttributes:nullptr]).get());
    return fragment;
}

bool Editor::WebContentReader::readImage(Ref<SharedBuffer>&& buffer, const String& type)
{
    ASSERT(type.contains('/'));
    String typeAsFilenameWithExtension = type;
    typeAsFilenameWithExtension.replace('/', '.');

    Vector<uint8_t> data;
    data.append(buffer->data(), buffer->size());
    auto blob = Blob::create(WTFMove(data), type);
    ASSERT(frame.document());
    String blobURL = DOMURL::createObjectURL(*frame.document(), blob);

    fragment = frame.editor().createFragmentForImageAndURL(blobURL);
    return fragment;
}

bool Editor::WebContentReader::readURL(const URL& url, const String& title)
{
    if (url.string().isEmpty())
        return false;

    auto anchor = HTMLAnchorElement::create(*frame.document());
    anchor->setAttributeWithoutSynchronization(HTMLNames::hrefAttr, url.string());
    anchor->appendChild(frame.document()->createTextNode([title precomposedStringWithCanonicalMapping]));

    fragment = frame.document()->createDocumentFragment();
    fragment->appendChild(anchor);
    return true;
}

bool Editor::WebContentReader::readPlainText(const String& text)
{
    if (!allowPlainText)
        return false;

    fragment = createFragmentFromText(context, [text precomposedStringWithCanonicalMapping]);
    if (!fragment)
        return false;

    madeFragmentFromPlainText = true;
    return true;
}

// FIXME: Should give this function a name that makes it clear it adds resources to the document loader as a side effect.
// Or refactor so it does not do that.
RefPtr<DocumentFragment> Editor::webContentFromPasteboard(Pasteboard& pasteboard, Range& context, bool allowPlainText, bool& chosePlainText)
{
    WebContentReader reader(m_frame, context, allowPlainText);
    pasteboard.read(reader);
    chosePlainText = reader.madeFragmentFromPlainText;
    return WTFMove(reader.fragment);
}

void Editor::applyFontStyles(const String& fontFamily, double fontSize, unsigned fontTraits)
{
    auto& cssValuePool = CSSValuePool::singleton();
    Ref<MutableStyleProperties> style = MutableStyleProperties::create();
    style->setProperty(CSSPropertyFontFamily, cssValuePool.createFontFamilyValue(fontFamily));
    style->setProperty(CSSPropertyFontStyle, (fontTraits & NSFontItalicTrait) ? CSSValueItalic : CSSValueNormal);
    style->setProperty(CSSPropertyFontWeight, (fontTraits & NSFontBoldTrait) ? CSSValueBold : CSSValueNormal);
    style->setProperty(CSSPropertyFontSize, cssValuePool.createValue(fontSize, CSSPrimitiveValue::CSS_PX));
    applyStyleToSelection(style.ptr(), EditActionSetFont);
}

} // namespace WebCore
