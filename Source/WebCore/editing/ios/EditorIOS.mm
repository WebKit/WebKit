/*
 * Copyright (C) 2006, 2007, 2013 Apple Inc. All rights reserved.
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
#include "Editor.h"

#include "CachedImage.h"
#include "Clipboard.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSPrimitiveValueMappings.h"
#include "DOMRangeInternal.h"
#include "DocumentFragment.h"
#include "DocumentLoader.h"
#include "EditorClient.h"
#include "Font.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "HTMLConverter.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLTextAreaElement.h"
#include "LegacyWebArchive.h"
#include "NodeTraversal.h"
#include "Page.h"
#include "Pasteboard.h"
#include "RenderBlock.h"
#include "RenderImage.h"
#include "SharedBuffer.h"
#include "SoftLinking.h"
#include "StyleProperties.h"
#include "Text.h"
#include "TypingCommand.h"
#include "WAKAppKitStubs.h"
#include "htmlediting.h"
#include "markup.h"

#if PLATFORM(IOS)

SOFT_LINK_FRAMEWORK(AppSupport)
SOFT_LINK(AppSupport, CPSharedResourcesDirectory, CFStringRef, (void), ())

SOFT_LINK_FRAMEWORK(MobileCoreServices)

SOFT_LINK(MobileCoreServices, UTTypeConformsTo, Boolean, (CFStringRef inUTI, CFStringRef inConformsToUTI), (inUTI, inConformsToUTI))
SOFT_LINK(MobileCoreServices, UTTypeCreatePreferredIdentifierForTag, CFStringRef, (CFStringRef inTagClass, CFStringRef inTag, CFStringRef inConformingToUTI), (inTagClass, inTag, inConformingToUTI))
SOFT_LINK(MobileCoreServices, UTTypeCopyPreferredTagWithClass, CFStringRef, (CFStringRef inUTI, CFStringRef inTagClass), (inUTI, inTagClass))

SOFT_LINK_CONSTANT(MobileCoreServices, kUTTypePNG, CFStringRef)
SOFT_LINK_CONSTANT(MobileCoreServices, kUTTypeJPEG, CFStringRef)
SOFT_LINK_CONSTANT(MobileCoreServices, kUTTagClassFilenameExtension, CFStringRef)
SOFT_LINK_CONSTANT(MobileCoreServices, kUTTagClassMIMEType, CFStringRef)

#define kUTTypePNG  getkUTTypePNG()
#define kUTTypeJPEG getkUTTypeJPEG()
#define kUTTagClassFilenameExtension getkUTTagClassFilenameExtension()
#define kUTTagClassMIMEType getkUTTagClassMIMEType()

@interface NSAttributedString (NSAttributedStringKitAdditions)
- (id)initWithRTF:(NSData *)data documentAttributes:(NSDictionary **)dict;
- (id)initWithRTFD:(NSData *)data documentAttributes:(NSDictionary **)dict;
- (NSData *)RTFFromRange:(NSRange)range documentAttributes:(NSDictionary *)dict;
- (NSData *)RTFDFromRange:(NSRange)range documentAttributes:(NSDictionary *)dict;
- (BOOL)containsAttachments;
@end

namespace WebCore {

using namespace HTMLNames;

void Editor::showFontPanel()
{
}

void Editor::showStylesPanel()
{
}

void Editor::showColorPanel()
{
}

void Editor::setTextAlignmentForChangedBaseWritingDirection(WritingDirection direction)
{
    // Note that the passed-in argument is the direction that has been changed to by
    // some code or user interaction outside the scope of this function. The former
    // direction is not known, nor is it required for the kind of text alignment
    // changes done by this function.
    //
    // Rules:
    // When text has no explicit alignment, set to alignment to match the writing direction.
    // If the text has left or right alignment, flip left->right and right->left. 
    // Otherwise, do nothing.

    RefPtr<EditingStyle> selectionStyle = EditingStyle::styleAtSelectionStart(m_frame.selection().selection());
    if (!selectionStyle || !selectionStyle->style())
         return;

    RefPtr<CSSPrimitiveValue> value = static_pointer_cast<CSSPrimitiveValue>(selectionStyle->style()->getPropertyCSSValue(CSSPropertyTextAlign));
    if (!value)
        return;
        
    const char *newValue = NULL;
    ETextAlign textAlign = *value;
    switch (textAlign) {
        case TASTART:
        case TAEND:
        {
            switch (direction) {
                case NaturalWritingDirection:
                    // no-op
                    break;
                case LeftToRightWritingDirection:
                    newValue = "left";
                    break;
                case RightToLeftWritingDirection:
                    newValue = "right";
                    break;
            }
            break;
        }
        case LEFT:
        case WEBKIT_LEFT:
            newValue = "right";
            break;
        case RIGHT:
        case WEBKIT_RIGHT:
            newValue = "left";
            break;
        case CENTER:
        case WEBKIT_CENTER:
        case JUSTIFY:
            // no-op
            break;
    }

    if (!newValue)
        return;

    Element* focusedElement = m_frame.document()->focusedElement();
    if (focusedElement && (focusedElement->hasTagName(textareaTag) || (focusedElement->hasTagName(inputTag) &&
        (toHTMLInputElement(focusedElement)->isTextField() ||
         toHTMLInputElement(focusedElement)->isSearchField())))) {
        if (direction == NaturalWritingDirection)
            return;
        toHTMLElement(focusedElement)->setAttribute(alignAttr, newValue);
        m_frame.document()->updateStyleIfNeeded();
        return;
    }

    RefPtr<MutableStyleProperties> style = MutableStyleProperties::create();
    style->setProperty(CSSPropertyTextAlign, newValue);
    applyParagraphStyle(style.get());
}

bool Editor::insertParagraphSeparatorInQuotedContent()
{
    // FIXME: Why is this missing calls to canEdit, canEditRichly, etc...
    TypingCommand::insertParagraphSeparatorInQuotedContent(*m_frame.document());
    revealSelectionAfterEditingOperation();
    return true;
}

// FIXME: Copied from EditorMac. This should be shared between the two so that
// the implementation does not differ.
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
    styleElement->setAttribute(styleAttr, styleText.impl());

    ExceptionCode ec = 0;
    styleElement->appendChild(frame->document()->createEditingTextNode(""), ec);
    ASSERT(!ec);

    position.deprecatedNode()->parentNode()->appendChild(styleElement, ec);
    ASSERT(!ec);

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

        if (nodeToRemove) {
            ExceptionCode ec;
            nodeToRemove->remove(ec);
            ASSERT(!ec);
        }

        return result;
    }

    const SimpleFontData* font = 0;
    RefPtr<Range> range = m_frame.selection().toNormalizedRange();
    if (Node* startNode = adjustedSelectionStartForStyleComputation(m_frame.selection().selection()).deprecatedNode()) {
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
    return result;
}

void Editor::removeUnchangeableStyles()
{
    // This function removes styles that the user cannot modify by applying their default values.
    
    RefPtr<EditingStyle> editingStyle = EditingStyle::create(m_frame.document()->body());
    RefPtr<MutableStyleProperties> defaultStyle = editingStyle.get()->style()->mutableCopy();
    
    // Text widgets implement background color via the UIView property. Their body element will not have one.
    defaultStyle->setProperty(CSSPropertyBackgroundColor, "rgba(255, 255, 255, 0.0)");
    
    // Remove properties that the user can modify, like font-weight. 
    // Also remove font-family, per HI spec.
    // FIXME: it'd be nice if knowledge about which styles were unchangeable was not hard-coded here.
    defaultStyle->removeProperty(CSSPropertyFontWeight);
    defaultStyle->removeProperty(CSSPropertyFontStyle);
    defaultStyle->removeProperty(CSSPropertyFontVariant);
    // FIXME: we should handle also pasted quoted text, strikethrough, etc. <rdar://problem/9255115>
    defaultStyle->removeProperty(CSSPropertyTextDecoration);
    defaultStyle->removeProperty(CSSPropertyWebkitTextDecorationsInEffect); // implements underline

    // FIXME add EditActionMatchStlye <rdar://problem/9156507> Undo rich text's paste & match style should say "Undo Match Style"
    applyStyleToSelection(defaultStyle.get(), EditActionChangeAttributes);
}

// FIXME: the following fuctions should be shared between Mac and iOS.
static NSAttributedString *attributedStringForRange(Range& range)
{
    return [adoptNS([[WebHTMLConverter alloc] initWithDOMRange:kit(&range)]) attributedString];
}

static PassRefPtr<SharedBuffer> dataInRTFDFormat(NSAttributedString *string)
{
    NSUInteger length = [string length];
    return length ? SharedBuffer::wrapNSData([string RTFDFromRange:NSMakeRange(0, length) documentAttributes:nil]) : nullptr;
}

static PassRefPtr<SharedBuffer> dataInRTFFormat(NSAttributedString *string)
{
    NSUInteger length = [string length];
    return length ? SharedBuffer::wrapNSData([string RTFFromRange:NSMakeRange(0, length) documentAttributes:nil]) : nullptr;
}    

String Editor::stringSelectionForPasteboardWithImageAltText()
{
    String text = selectedTextForClipboard();
    text.replace(noBreakSpace, ' ');
    return text;
}

PassRefPtr<SharedBuffer> Editor::selectionInWebArchiveFormat()
{
    RefPtr<LegacyWebArchive> archive = LegacyWebArchive::createFromSelection(&m_frame);
    return archive ? SharedBuffer::wrapCFData(archive->rawDataRepresentation().get()) : nullptr;
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

void Editor::writeImageToPasteboard(Pasteboard& pasteboard, Element& imageElement, const URL&, const String& title)
{
    PasteboardImage pasteboardImage;

    CachedImage* cachedImage;
    getImage(imageElement, pasteboardImage.image, cachedImage);
    if (!pasteboardImage.image)
        return;
    ASSERT(cachedImage);

    pasteboardImage.url.url = imageElement.document().completeURL(stripLeadingAndTrailingHTMLSpaces(imageElement.imageSourceURL()));
    pasteboardImage.url.title = title;
    pasteboardImage.resourceMIMEType = pasteboard.resourceMIMEType(cachedImage->response().mimeType());

    pasteboard.write(pasteboardImage);
}

class Editor::WebContentReader final : public PasteboardWebContentReader {
public:
    WebContentReader(Frame& frame, Range& context, bool allowPlainText)
        : frame(frame)
        , context(context)
        , allowPlainText(allowPlainText)
        , madeFragmentFromPlainText(false)
    {
    }

    Frame& frame;
    Range& context;
    const bool allowPlainText;

    RefPtr<DocumentFragment> fragment;
    bool madeFragmentFromPlainText;

private:
    virtual bool readWebArchive(PassRefPtr<SharedBuffer>) override;
    virtual bool readFilenames(const Vector<String>&) override;
    virtual bool readHTML(const String&) override;
    virtual bool readRTFD(PassRefPtr<SharedBuffer>) override;
    virtual bool readRTF(PassRefPtr<SharedBuffer>) override;
    virtual bool readImage(PassRefPtr<SharedBuffer>, const String& type) override;
    virtual bool readURL(const URL&, const String& title) override;
    virtual bool readPlainText(const String&) override;
    void addFragment(PassRefPtr<DocumentFragment>);
};

void Editor::WebContentReader::addFragment(PassRefPtr<DocumentFragment> newFragment)
{
    if (fragment) {
        if (newFragment && newFragment->firstChild()) {
            ExceptionCode ec;
            fragment->appendChild(newFragment->firstChild(), ec);
        }
    } else
        fragment = newFragment;
}

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
        addFragment(createFragmentFromMarkup(*frame.document(), markupString, mainResource->url(), DisallowScriptingAndPluginContent));
        return true;
    }

    return false;
}

bool Editor::WebContentReader::readFilenames(const Vector<String>&)
{
    return false;
}

bool Editor::WebContentReader::readHTML(const String&)
{
    return false;
}

bool Editor::WebContentReader::readRTFD(PassRefPtr<SharedBuffer> buffer)
{
    addFragment(frame.editor().createFragmentAndAddResources(adoptNS([[NSAttributedString alloc] initWithRTFD:buffer->createNSData().get() documentAttributes:nullptr]).get()));
    return fragment;
}

bool Editor::WebContentReader::readRTF(PassRefPtr<SharedBuffer> buffer)
{
    addFragment(frame.editor().createFragmentAndAddResources(adoptNS([[NSAttributedString alloc] initWithRTF:buffer->createNSData().get() documentAttributes:nullptr]).get()));
    return fragment;
}

static NSURL* uniqueURLWithRelativePart(NSString *relativePart)
{
    RetainPtr<CFUUIDRef> UUIDRef = adoptCF(CFUUIDCreate(kCFAllocatorDefault));
    RetainPtr<NSString> UUIDString = adoptNS((NSString *)CFUUIDCreateString(kCFAllocatorDefault, UUIDRef.get()));

    return [NSURL URLWithString:[NSString stringWithFormat:@"%@://%@/%@", @"webkit-fake-url", UUIDString.get(), relativePart]];
}

bool Editor::WebContentReader::readImage(PassRefPtr<SharedBuffer> buffer, const String& type)
{
    RetainPtr<CFStringRef> stringType = type.createCFString();
    RetainPtr<NSString> filenameExtension = adoptNS((NSString *)UTTypeCopyPreferredTagWithClass(stringType.get(), kUTTagClassFilenameExtension));
    NSString *relativeURLPart = [@"image" stringByAppendingString:filenameExtension.get()];
    RetainPtr<NSString> mimeType = adoptNS((NSString *)UTTypeCopyPreferredTagWithClass(stringType.get(), kUTTagClassMIMEType));

    addFragment(frame.editor().createFragmentForImageResourceAndAddResource(ArchiveResource::create(buffer, uniqueURLWithRelativePart(relativeURLPart), mimeType.get(), emptyString(), emptyString())));
    return fragment;
}

bool Editor::WebContentReader::readURL(const URL& url, const String&)
{
    if (url.isEmpty())
        return false;

    if (!frame.editor().client()->hasRichlyEditableSelection()) {
        if (readPlainText([(NSURL *)url absoluteString]))
            return true;
    }

    if ([(NSURL *)url isFileURL]) {
        NSString *localPath = [(NSURL *)url relativePath];
        // Only allow url attachments from ~/Media for now.
        if (![localPath hasPrefix:[(NSString *)CPSharedResourcesDirectory() stringByAppendingString:@"/Media/DCIM/"]])
            return false;

        RetainPtr<NSString> fileType = adoptNS((NSString *)UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, (CFStringRef)[localPath pathExtension], NULL));
        NSData *data = [NSData dataWithContentsOfFile:localPath];
        if (UTTypeConformsTo((CFStringRef)fileType.get(), kUTTypePNG)) {
            addFragment(frame.editor().createFragmentForImageResourceAndAddResource(ArchiveResource::create(SharedBuffer::wrapNSData([[data copy] autorelease]), uniqueURLWithRelativePart(@"image.png"), @"image/png", emptyString(), emptyString())));
            return fragment;
        } else if (UTTypeConformsTo((CFStringRef)fileType.get(), kUTTypeJPEG)) {
            addFragment(frame.editor().createFragmentForImageResourceAndAddResource(ArchiveResource::create(SharedBuffer::wrapNSData([[data copy] autorelease]), uniqueURLWithRelativePart(@"image.jpg"), @"image/jpg", emptyString(), emptyString())));
            return fragment;
        }
    } else {
        RefPtr<Element> anchor = frame.document()->createElement(HTMLNames::aTag, false);
        anchor->setAttribute(HTMLNames::hrefAttr, url.string());
        anchor->appendChild(frame.document()->createTextNode([[(NSURL *)url absoluteString] precomposedStringWithCanonicalMapping]));

        RefPtr<DocumentFragment> newFragment = frame.document()->createDocumentFragment();
        newFragment->appendChild(anchor.release());
        addFragment(newFragment);
        return true;
    }
    return false;
}

bool Editor::WebContentReader::readPlainText(const String& text)
{
    if (!allowPlainText)
        return false;

    addFragment(createFragmentFromText(context, [text precomposedStringWithCanonicalMapping]));
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

void Editor::pasteWithPasteboard(Pasteboard* pasteboard, bool allowPlainText)
{
    RefPtr<Range> range = selectedRange();

    bool chosePlainText;
    RefPtr<DocumentFragment> fragment = client()->documentFragmentFromDelegate(0);
    if (!fragment)
        fragment = webContentFromPasteboard(*pasteboard, *range, allowPlainText, chosePlainText);

    if (fragment && shouldInsertFragment(fragment, range, EditorInsertActionPasted))
        pasteAsFragment(fragment, canSmartReplaceWithPasteboard(*pasteboard), false);
}

PassRefPtr<DocumentFragment> Editor::createFragmentAndAddResources(NSAttributedString *string)
{
    if (!m_frame.page() || !m_frame.document() || !m_frame.document()->isHTMLDocument())
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

PassRefPtr<DocumentFragment> Editor::createFragmentForImageResourceAndAddResource(PassRefPtr<ArchiveResource> resource)
{
    if (!resource)
        return nullptr;

    RefPtr<Element> imageElement = m_frame.document()->createElement(HTMLNames::imgTag, false);
    // FIXME: The code in createFragmentAndAddResources calls setDefersLoading(true). Don't we need that here?
    if (DocumentLoader* loader = m_frame.loader().documentLoader())
        loader->addArchiveResource(resource.get());

    NSURL *URL = resource->url();
    imageElement->setAttribute(HTMLNames::srcAttr, [URL isFileURL] ? [URL absoluteString] : resource->url());

    RefPtr<DocumentFragment> fragment = m_frame.document()->createDocumentFragment();
    fragment->appendChild(imageElement.release());

    return fragment.release();
}

} // namespace WebCore

#endif // PLATFORM(IOS)
