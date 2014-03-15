/*
 * Copyright (C) 2006, 2007, 2008, 2013 Apple Inc. All rights reserved.
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

#import "CachedResourceLoader.h"
#import "Clipboard.h"
#import "ColorMac.h"
#import "DOMRangeInternal.h"
#import "DocumentFragment.h"
#import "DocumentLoader.h"
#import "Editor.h"
#import "EditorClient.h"
#import "Font.h"
#import "Frame.h"
#import "FrameLoaderClient.h"
#import "FrameView.h"
#import "HTMLConverter.h"
#import "HTMLElement.h"
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
#import "ResourceBuffer.h"
#import "RuntimeApplicationChecks.h"
#import "Sound.h"
#import "StyleProperties.h"
#import "Text.h"
#import "TypingCommand.h"
#import "UUID.h"
#import "WebNSAttributedStringExtras.h"
#import "htmlediting.h"
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

void Editor::pasteWithPasteboard(Pasteboard* pasteboard, bool allowPlainText)
{
    RefPtr<Range> range = selectedRange();

    // FIXME: How can this hard-coded pasteboard name be right, given that the passed-in pasteboard has a name?
    client()->setInsertionPasteboard(NSGeneralPboard);

    bool chosePlainText;
    RefPtr<DocumentFragment> fragment = webContentFromPasteboard(*pasteboard, *range, allowPlainText, chosePlainText);

    if (fragment && shouldInsertFragment(fragment, range, EditorInsertActionPasted))
        pasteAsFragment(fragment, canSmartReplaceWithPasteboard(*pasteboard), false);

    client()->setInsertionPasteboard(String());
}

bool Editor::insertParagraphSeparatorInQuotedContent()
{
    // FIXME: Why is this missing calls to canEdit, canEditRichly, etc.?
    TypingCommand::insertParagraphSeparatorInQuotedContent(document());
    revealSelectionAfterEditingOperation();
    return true;
}

static RenderStyle* styleForSelectionStart(Frame* frame, Node *&nodeToRemove)
{
    nodeToRemove = 0;

    if (frame->selection().isNone())
        return 0;

    Position position = frame->selection().selection().visibleStart().deepEquivalent();
    if (!position.isCandidate() || position.isNull())
        return 0;

    RefPtr<EditingStyle> typingStyle = frame->selection().typingStyle();
    if (!typingStyle || !typingStyle->style())
        return &position.deprecatedNode()->renderer()->style();

    RefPtr<Element> styleElement = frame->document()->createElement(spanTag, false);

    String styleText = typingStyle->style()->asText() + " display: inline";
    styleElement->setAttribute(styleAttr, styleText);

    styleElement->appendChild(frame->document()->createEditingTextNode(""), ASSERT_NO_EXCEPTION);

    position.deprecatedNode()->parentNode()->appendChild(styleElement, ASSERT_NO_EXCEPTION);

    nodeToRemove = styleElement.get();
    return styleElement->renderer() ? &styleElement->renderer()->style() : 0;
}

const SimpleFontData* Editor::fontForSelection(bool& hasMultipleFonts) const
{
    hasMultipleFonts = false;

    if (!m_frame.selection().isRange()) {
        Node* nodeToRemove;
        RenderStyle* style = styleForSelectionStart(&m_frame, nodeToRemove); // sets nodeToRemove

        const SimpleFontData* result = 0;
        if (style)
            result = style->font().primaryFont();

        if (nodeToRemove)
            nodeToRemove->remove(ASSERT_NO_EXCEPTION);

        return result;
    }

    const SimpleFontData* font = 0;
    RefPtr<Range> range = m_frame.selection().toNormalizedRange();
    Node* startNode = adjustedSelectionStartForStyleComputation(m_frame.selection().selection()).deprecatedNode();
    if (range && startNode) {
        Node* pastEnd = range->pastLastNode();
        // In the loop below, n should eventually match pastEnd and not become nil, but we've seen at least one
        // unreproducible case where this didn't happen, so check for null also.
        for (Node* node = startNode; node && node != pastEnd; node = NodeTraversal::next(node)) {
            auto renderer = node->renderer();
            if (!renderer)
                continue;
            // FIXME: Are there any node types that have renderers, but that we should be skipping?
            const SimpleFontData* primaryFont = renderer->style().font().primaryFont();
            if (!font)
                font = primaryFont;
            else if (font != primaryFont) {
                hasMultipleFonts = true;
                break;
            }
        }
    }

    return font;
}

NSDictionary* Editor::fontAttributesForSelectionStart() const
{
    Node* nodeToRemove;
    RenderStyle* style = styleForSelectionStart(&m_frame, nodeToRemove);
    if (!style)
        return nil;

    NSMutableDictionary* result = [NSMutableDictionary dictionary];

    if (style->visitedDependentColor(CSSPropertyBackgroundColor).isValid() && style->visitedDependentColor(CSSPropertyBackgroundColor).alpha() != 0)
        [result setObject:nsColor(style->visitedDependentColor(CSSPropertyBackgroundColor)) forKey:NSBackgroundColorAttributeName];

    if (style->font().primaryFont()->getNSFont())
        [result setObject:style->font().primaryFont()->getNSFont() forKey:NSFontAttributeName];

    if (style->visitedDependentColor(CSSPropertyColor).isValid() && style->visitedDependentColor(CSSPropertyColor) != Color::black)
        [result setObject:nsColor(style->visitedDependentColor(CSSPropertyColor)) forKey:NSForegroundColorAttributeName];

    const ShadowData* shadow = style->textShadow();
    if (shadow) {
        RetainPtr<NSShadow> s = adoptNS([[NSShadow alloc] init]);
        [s.get() setShadowOffset:NSMakeSize(shadow->x(), shadow->y())];
        [s.get() setShadowBlurRadius:shadow->radius()];
        [s.get() setShadowColor:nsColor(shadow->color())];
        [result setObject:s.get() forKey:NSShadowAttributeName];
    }

    int decoration = style->textDecorationsInEffect();
    if (decoration & TextDecorationLineThrough)
        [result setObject:[NSNumber numberWithInt:NSUnderlineStyleSingle] forKey:NSStrikethroughStyleAttributeName];

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
        [result setObject:[NSNumber numberWithInt:superscriptInt] forKey:NSSuperscriptAttributeName];

    if (decoration & TextDecorationUnderline)
        [result setObject:[NSNumber numberWithInt:NSUnderlineStyleSingle] forKey:NSUnderlineStyleAttributeName];

    if (nodeToRemove)
        nodeToRemove->remove(ASSERT_NO_EXCEPTION);

    return result;
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
    platformStrategies()->pasteboardStrategy()->setTypes(types, NSFindPboard);
    platformStrategies()->pasteboardStrategy()->setStringForType(m_frame.displayStringModifiedByEncoding(selectedTextForClipboard()), NSStringPboardType, NSFindPboard);
}

void Editor::readSelectionFromPasteboard(const String& pasteboardName)
{
    Pasteboard pasteboard(pasteboardName);
    if (m_frame.selection().selection().isContentRichlyEditable())
        pasteWithPasteboard(&pasteboard, true);
    else
        pasteAsPlainTextWithPasteboard(pasteboard);
}

// FIXME: Makes no sense that selectedTextForClipboard always includes alt text, but stringSelectionForPasteboard does not.
// This was left in a bad state when selectedTextForClipboard was added. Need to look over clients and fix this.
String Editor::stringSelectionForPasteboard()
{
    if (!canCopy())
        return "";
    String text = selectedText();
    text.replace(noBreakSpace, ' ');
    return text;
}

String Editor::stringSelectionForPasteboardWithImageAltText()
{
    if (!canCopy())
        return "";
    String text = selectedTextForClipboard();
    text.replace(noBreakSpace, ' ');
    return text;
}

PassRefPtr<SharedBuffer> Editor::selectionInWebArchiveFormat()
{
    RefPtr<LegacyWebArchive> archive = LegacyWebArchive::createFromSelection(&m_frame);
    return archive ? SharedBuffer::wrapCFData(archive->rawDataRepresentation().get()) : 0;
}

PassRefPtr<Range> Editor::adjustedSelectionRange()
{
    // FIXME: Why do we need to adjust the selection to include the anchor tag it's in?
    // Whoever wrote this code originally forgot to leave us a comment explaining the rationale.
    RefPtr<Range> range = selectedRange();
    Node* commonAncestor = range->commonAncestorContainer(IGNORE_EXCEPTION);
    ASSERT(commonAncestor);
    Node* enclosingAnchor = enclosingNodeWithTag(firstPositionInNode(commonAncestor), HTMLNames::aTag);
    if (enclosingAnchor && comparePositions(firstPositionInOrBeforeNode(range->startPosition().anchorNode()), range->startPosition()) >= 0)
        range->setStart(enclosingAnchor, 0, IGNORE_EXCEPTION);
    return range;
}

static NSAttributedString *attributedStringForRange(Range& range)
{
    return [adoptNS([[WebHTMLConverter alloc] initWithDOMRange:kit(&range)]) attributedString];
}

static PassRefPtr<SharedBuffer> dataInRTFDFormat(NSAttributedString *string)
{
    NSUInteger length = [string length];
    return length ? SharedBuffer::wrapNSData([string RTFDFromRange:NSMakeRange(0, length) documentAttributes:nil]) : 0;
}

static PassRefPtr<SharedBuffer> dataInRTFFormat(NSAttributedString *string)
{
    NSUInteger length = [string length];
    return length ? SharedBuffer::wrapNSData([string RTFFromRange:NSMakeRange(0, length) documentAttributes:nil]) : 0;
}

PassRefPtr<SharedBuffer> Editor::dataSelectionForPasteboard(const String& pasteboardType)
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
       return dataInRTFDFormat(attributedStringForRange(*adjustedSelectionRange()));

    if (pasteboardType == String(NSRTFPboardType)) {
        NSAttributedString* attributedString = attributedStringForRange(*adjustedSelectionRange());
        // FIXME: Why is this attachment character stripping needed here, but not needed in writeSelectionToPasteboard?
        if ([attributedString containsAttachments])
            attributedString = attributedStringByStrippingAttachmentCharacters(attributedString);
        return dataInRTFFormat(attributedString);
    }

    return 0;
}

void Editor::writeSelectionToPasteboard(Pasteboard& pasteboard)
{
    NSAttributedString *attributedString = attributedStringForRange(*selectedRange());

    PasteboardWebContent content;
    content.canSmartCopyOrDelete = canSmartCopyOrDelete();
    content.dataInWebArchiveFormat = selectionInWebArchiveFormat();
    content.dataInRTFDFormat = [attributedString containsAttachments] ? dataInRTFDFormat(attributedString) : 0;
    content.dataInRTFFormat = dataInRTFFormat(attributedString);
    content.dataInStringFormat = stringSelectionForPasteboardWithImageAltText();
    client()->getClientPasteboardDataForRange(selectedRange().get(), content.clientTypes, content.clientData);

    pasteboard.write(content);
}

static void getImage(Element& imageElement, RefPtr<Image>& image, CachedImage*& cachedImage)
{
    auto renderer = imageElement.renderer();
    if (!renderer || !renderer->isRenderImage())
        return;

    CachedImage* tentativeCachedImage = toRenderImage(renderer)->cachedImage();
    if (!tentativeCachedImage || tentativeCachedImage->errorOccurred()) {
        tentativeCachedImage = 0;
        return;
    }

    image = tentativeCachedImage->imageForRenderer(renderer);
    if (!image)
        return;

    cachedImage = tentativeCachedImage;
}

void Editor::fillInUserVisibleForm(PasteboardURL& pasteboardURL)
{
    pasteboardURL.userVisibleForm = client()->userVisibleString(pasteboardURL.url);
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

    pasteboardImage.url.url = url;
    pasteboardImage.url.title = title;
    pasteboardImage.url.userVisibleForm = client()->userVisibleString(pasteboardImage.url.url);
    pasteboardImage.resourceData = cachedImage->resourceBuffer()->sharedBuffer();
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
    virtual bool readWebArchive(PassRefPtr<SharedBuffer>) override;
    virtual bool readFilenames(const Vector<String>&) override;
    virtual bool readHTML(const String&) override;
    virtual bool readRTFD(PassRefPtr<SharedBuffer>) override;
    virtual bool readRTF(PassRefPtr<SharedBuffer>) override;
    virtual bool readImage(PassRefPtr<SharedBuffer>, const String& type) override;
    virtual bool readURL(const URL&, const String& title) override;
    virtual bool readPlainText(const String&) override;
};

bool Editor::WebContentReader::readWebArchive(PassRefPtr<SharedBuffer> buffer)
{
    if (!frame.document())
        return false;

    RefPtr<LegacyWebArchive> archive = LegacyWebArchive::create(URL(), buffer.get());
    if (!archive)
        return false;

    RefPtr<ArchiveResource> mainResource = archive->mainResource();
    if (!mainResource)
        return false;

    const String& type = mainResource->mimeType();

    if (frame.loader().client().canShowMIMETypeAsHTML(type)) {
        // FIXME: The code in createFragmentAndAddResources calls setDefersLoading(true). Don't we need that here?
        if (DocumentLoader* loader = frame.loader().documentLoader())
            loader->addAllArchiveResources(archive.get());

        String markupString = String::fromUTF8(mainResource->data()->data(), mainResource->data()->size());
        fragment = createFragmentFromMarkup(*frame.document(), markupString, mainResource->url(), DisallowScriptingAndPluginContent);
        return true;
    }

    if (MIMETypeRegistry::isSupportedImageMIMEType(type)) {
        fragment = frame.editor().createFragmentForImageResourceAndAddResource(mainResource.release());
        return true;
    }

    return false;
}

bool Editor::WebContentReader::readFilenames(const Vector<String>& paths)
{
    size_t size = paths.size();
    if (!size)
        return false;

    if (!frame.document())
        return false;
    Document& document = *frame.document();

    fragment = document.createDocumentFragment();

    for (size_t i = 0; i < size; i++) {
        String text = paths[i];
        text = frame.editor().client()->userVisibleString([NSURL fileURLWithPath:text]);

        RefPtr<HTMLElement> paragraph = createDefaultParagraphElement(document);
        paragraph->appendChild(document.createTextNode(text));
        fragment->appendChild(paragraph.release());
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

bool Editor::WebContentReader::readRTFD(PassRefPtr<SharedBuffer> buffer)
{
    fragment = frame.editor().createFragmentAndAddResources(adoptNS([[NSAttributedString alloc] initWithRTFD:buffer->createNSData().get() documentAttributes:nullptr]).get());
    return fragment;
}

bool Editor::WebContentReader::readRTF(PassRefPtr<SharedBuffer> buffer)
{
    fragment = frame.editor().createFragmentAndAddResources(adoptNS([[NSAttributedString alloc] initWithRTF:buffer->createNSData().get() documentAttributes:nullptr]).get());
    return fragment;
}

bool Editor::WebContentReader::readImage(PassRefPtr<SharedBuffer> buffer, const String& type)
{
    ASSERT(type.contains('/'));
    String typeAsFilenameWithExtension = type;
    typeAsFilenameWithExtension.replace('/', '.');
    URL imageURL = URL::fakeURLWithRelativePart(typeAsFilenameWithExtension);

    fragment = frame.editor().createFragmentForImageResourceAndAddResource(ArchiveResource::create(buffer, imageURL, type, emptyString(), emptyString()));
    return fragment;
}

bool Editor::WebContentReader::readURL(const URL& url, const String& title)
{
    if (url.string().isEmpty())
        return false;

    RefPtr<Element> anchor = frame.document()->createElement(HTMLNames::aTag, false);
    anchor->setAttribute(HTMLNames::hrefAttr, url.string());
    anchor->appendChild(frame.document()->createTextNode([title precomposedStringWithCanonicalMapping]));

    fragment = frame.document()->createDocumentFragment();
    fragment->appendChild(anchor.release());
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
PassRefPtr<DocumentFragment> Editor::webContentFromPasteboard(Pasteboard& pasteboard, Range& context, bool allowPlainText, bool& chosePlainText)
{
    WebContentReader reader(m_frame, context, allowPlainText);
    pasteboard.read(reader);
    chosePlainText = reader.madeFragmentFromPlainText;
    return reader.fragment.release();
}

PassRefPtr<DocumentFragment> Editor::createFragmentForImageResourceAndAddResource(PassRefPtr<ArchiveResource> resource)
{
    if (!resource)
        return nullptr;

    RefPtr<Element> imageElement = document().createElement(HTMLNames::imgTag, false);
    imageElement->setAttribute(HTMLNames::srcAttr, resource->url().string());

    RefPtr<DocumentFragment> fragment = document().createDocumentFragment();
    fragment->appendChild(imageElement.release());

    // FIXME: The code in createFragmentAndAddResources calls setDefersLoading(true). Don't we need that here?
    if (DocumentLoader* loader = m_frame.loader().documentLoader())
        loader->addArchiveResource(resource.get());

    return fragment.release();
}

PassRefPtr<DocumentFragment> Editor::createFragmentAndAddResources(NSAttributedString *string)
{
    if (!m_frame.page() || !document().isHTMLDocument())
        return nullptr;

    if (!string)
        return nullptr;

    bool wasDeferringCallbacks = m_frame.page()->defersLoading();
    if (!wasDeferringCallbacks)
        m_frame.page()->setDefersLoading(true);

    Vector<RefPtr<ArchiveResource>> resources;
    RefPtr<DocumentFragment> fragment = client()->documentFragmentFromAttributedString(string, resources);

    if (DocumentLoader* loader = m_frame.loader().documentLoader()) {
        for (size_t i = 0, size = resources.size(); i < size; ++i)
            loader->addArchiveResource(resources[i]);
    }

    if (!wasDeferringCallbacks)
        m_frame.page()->setDefersLoading(false);

    return fragment.release();
}

} // namespace WebCore
