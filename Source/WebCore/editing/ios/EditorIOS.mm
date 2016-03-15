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

#include "BlockExceptions.h"
#include "CachedImage.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSPrimitiveValueMappings.h"
#include "DOMRangeInternal.h"
#include "DataTransfer.h"
#include "DocumentFragment.h"
#include "DocumentLoader.h"
#include "EditorClient.h"
#include "FontCascade.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "HTMLConverter.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLTextAreaElement.h"
#include "LegacyWebArchive.h"
#include "NSAttributedStringSPI.h"
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
        
    const char *newValue = nullptr;
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
    if (focusedElement && (is<HTMLTextAreaElement>(*focusedElement) || (is<HTMLInputElement>(*focusedElement)
        && (downcast<HTMLInputElement>(*focusedElement).isTextField()
            || downcast<HTMLInputElement>(*focusedElement).isSearchField())))) {
        if (direction == NaturalWritingDirection)
            return;
        downcast<HTMLElement>(*focusedElement).setAttribute(alignAttr, newValue);
        m_frame.document()->updateStyleIfNeeded();
        return;
    }

    RefPtr<MutableStyleProperties> style = MutableStyleProperties::create();
    style->setProperty(CSSPropertyTextAlign, newValue);
    applyParagraphStyle(style.get());
}

const Font* Editor::fontForSelection(bool& hasMultipleFonts) const
{
    hasMultipleFonts = false;

    if (!m_frame.selection().isRange()) {
        Node* nodeToRemove;
        RenderStyle* style = styleForSelectionStart(&m_frame, nodeToRemove); // sets nodeToRemove

        const Font* result = nullptr;
        if (style) {
            result = &style->fontCascade().primaryFont();
            if (nodeToRemove)
                nodeToRemove->remove(ASSERT_NO_EXCEPTION);
        }

        return result;
    }

    const Font* font = nullptr;
    RefPtr<Range> range = m_frame.selection().toNormalizedRange();
    if (Node* startNode = adjustedSelectionStartForStyleComputation(m_frame.selection().selection()).deprecatedNode()) {
        Node* pastEnd = range->pastLastNode();
        // In the loop below, n should eventually match pastEnd and not become nil, but we've seen at least one
        // unreproducible case where this didn't happen, so check for null also.
        for (Node* node = startNode; node && node != pastEnd; node = NodeTraversal::next(*node)) {
            auto renderer = node->renderer();
            if (!renderer)
                continue;
            // FIXME: Are there any node types that have renderers, but that we should be skipping?
            const Font& primaryFont = renderer->style().fontCascade().primaryFont();
            if (!font)
                font = &primaryFont;
            else if (font != &primaryFont) {
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
    
    CTFontRef font = style->fontCascade().primaryFont().getCTFont();
    if (font)
        [result setObject:(id)font forKey:NSFontAttributeName];

    getTextDecorationAttributesRespectingTypingStyle(*style, result);

    if (nodeToRemove)
        nodeToRemove->remove(ASSERT_NO_EXCEPTION);
    
    return result;
}

void Editor::removeUnchangeableStyles()
{
    // This function removes styles that the user cannot modify by applying their default values.
    
    RefPtr<EditingStyle> editingStyle = EditingStyle::create(m_frame.document()->bodyOrFrameset());
    RefPtr<MutableStyleProperties> defaultStyle = editingStyle.get()->style()->mutableCopy();
    
    // Text widgets implement background color via the UIView property. Their body element will not have one.
    defaultStyle->setProperty(CSSPropertyBackgroundColor, "rgba(255, 255, 255, 0.0)");
    
    // Remove properties that the user can modify, like font-weight. 
    // Also remove font-family, per HI spec.
    // FIXME: it'd be nice if knowledge about which styles were unchangeable was not hard-coded here.
    defaultStyle->removeProperty(CSSPropertyFontWeight);
    defaultStyle->removeProperty(CSSPropertyFontStyle);
    defaultStyle->removeProperty(CSSPropertyFontVariantCaps);
    // FIXME: we should handle also pasted quoted text, strikethrough, etc. <rdar://problem/9255115>
    defaultStyle->removeProperty(CSSPropertyTextDecoration);
    defaultStyle->removeProperty(CSSPropertyWebkitTextDecorationsInEffect); // implements underline

    // FIXME add EditActionMatchStlye <rdar://problem/9156507> Undo rich text's paste & match style should say "Undo Match Style"
    applyStyleToSelection(defaultStyle.get(), EditActionChangeAttributes);
}

static RefPtr<SharedBuffer> dataInRTFDFormat(NSAttributedString *string)
{
    NSUInteger length = string.length;
    if (!length)
        return nullptr;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return SharedBuffer::wrapNSData([string RTFDFromRange:NSMakeRange(0, length) documentAttributes:nil]);
    END_BLOCK_OBJC_EXCEPTIONS;

    return nullptr;
}

static RefPtr<SharedBuffer> dataInRTFFormat(NSAttributedString *string)
{
    NSUInteger length = string.length;
    if (!length)
        return nullptr;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return SharedBuffer::wrapNSData([string RTFFromRange:NSMakeRange(0, length) documentAttributes:nil]);
    END_BLOCK_OBJC_EXCEPTIONS;

    return nullptr;
}

String Editor::stringSelectionForPasteboardWithImageAltText()
{
    String text = selectedTextForDataTransfer();
    text.replace(noBreakSpace, ' ');
    return text;
}

RefPtr<SharedBuffer> Editor::selectionInWebArchiveFormat()
{
    RefPtr<LegacyWebArchive> archive = LegacyWebArchive::createFromSelection(&m_frame);
    if (!archive)
        return nullptr;
    return SharedBuffer::wrapCFData(archive->rawDataRepresentation().get());
}

void Editor::writeSelectionToPasteboard(Pasteboard& pasteboard)
{
    NSAttributedString *attributedString = attributedStringFromRange(*selectedRange());

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
    pasteboardImage.resourceData = cachedImage->resourceBuffer();

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

    void addFragment(RefPtr<DocumentFragment>&&);

    Frame& frame;
    Range& context;
    const bool allowPlainText;

    RefPtr<DocumentFragment> fragment;
    bool madeFragmentFromPlainText;

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

void Editor::WebContentReader::addFragment(RefPtr<DocumentFragment>&& newFragment)
{
    if (fragment) {
        if (newFragment && newFragment->firstChild()) {
            ExceptionCode ec;
            fragment->appendChild(*newFragment->firstChild(), ec);
        }
    } else
        fragment = WTFMove(newFragment);
}

bool Editor::WebContentReader::readWebArchive(SharedBuffer* buffer)
{
    if (!frame.document())
        return false;

    if (!buffer)
        return false;

    RefPtr<LegacyWebArchive> archive = LegacyWebArchive::create(URL(), *buffer);
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

        String markupString = String::fromUTF8(mainResource->data().data(), mainResource->data().size());
        addFragment(createFragmentFromMarkup(*frame.document(), markupString, mainResource->url(), DisallowScriptingAndPluginContent));
        return true;
    }

    return false;
}

bool Editor::WebContentReader::readFilenames(const Vector<String>&)
{
    return false;
}

bool Editor::WebContentReader::readHTML(const String& string)
{
    if (!frame.document())
        return false;

    addFragment(createFragmentFromMarkup(*frame.document(), string, emptyString(), DisallowScriptingAndPluginContent));
    return true;
}

bool Editor::WebContentReader::readRTFD(SharedBuffer& buffer)
{
    addFragment(frame.editor().createFragmentAndAddResources(adoptNS([[NSAttributedString alloc] initWithRTFD:buffer.createNSData().get() documentAttributes:nullptr]).get()));
    return fragment;
}

bool Editor::WebContentReader::readRTF(SharedBuffer& buffer)
{
    addFragment(frame.editor().createFragmentAndAddResources(adoptNS([[NSAttributedString alloc] initWithRTF:buffer.createNSData().get() documentAttributes:nullptr]).get()));
    return fragment;
}

bool Editor::WebContentReader::readImage(Ref<SharedBuffer>&& buffer, const String& type)
{
    RetainPtr<CFStringRef> stringType = type.createCFString();
    RetainPtr<NSString> filenameExtension = adoptNS((NSString *)UTTypeCopyPreferredTagWithClass(stringType.get(), kUTTagClassFilenameExtension));
    NSString *relativeURLPart = [@"image" stringByAppendingString:filenameExtension.get()];
    RetainPtr<NSString> mimeType = adoptNS((NSString *)UTTypeCopyPreferredTagWithClass(stringType.get(), kUTTagClassMIMEType));

    addFragment(frame.editor().createFragmentForImageResourceAndAddResource(ArchiveResource::create(WTFMove(buffer), URL::fakeURLWithRelativePart(relativeURLPart), mimeType.get(), emptyString(), emptyString())));
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
            addFragment(frame.editor().createFragmentForImageResourceAndAddResource(ArchiveResource::create(SharedBuffer::wrapNSData([[data copy] autorelease]), URL::fakeURLWithRelativePart("image.png"), @"image/png", emptyString(), emptyString())));
            return fragment;
        } else if (UTTypeConformsTo((CFStringRef)fileType.get(), kUTTypeJPEG)) {
            addFragment(frame.editor().createFragmentForImageResourceAndAddResource(ArchiveResource::create(SharedBuffer::wrapNSData([[data copy] autorelease]), URL::fakeURLWithRelativePart("image.jpg"), @"image/jpg", emptyString(), emptyString())));
            return fragment;
        }
    } else {
        auto anchor = frame.document()->createElement(HTMLNames::aTag, false);
        anchor->setAttribute(HTMLNames::hrefAttr, url.string());
        anchor->appendChild(frame.document()->createTextNode([[(NSURL *)url absoluteString] precomposedStringWithCanonicalMapping]));

        auto newFragment = frame.document()->createDocumentFragment();
        newFragment->appendChild(WTFMove(anchor));
        addFragment(WTFMove(newFragment));
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
RefPtr<DocumentFragment> Editor::webContentFromPasteboard(Pasteboard& pasteboard, Range& context, bool allowPlainText, bool& chosePlainText)
{
    WebContentReader reader(m_frame, context, allowPlainText);
    pasteboard.read(reader);
    chosePlainText = reader.madeFragmentFromPlainText;
    return WTFMove(reader.fragment);
}

void Editor::pasteWithPasteboard(Pasteboard* pasteboard, bool allowPlainText, MailBlockquoteHandling mailBlockquoteHandling)
{
    RefPtr<Range> range = selectedRange();
    WebContentReader reader(m_frame, *range, allowPlainText);
    int numberOfPasteboardItems = client()->getPasteboardItemsCount();
    for (int i = 0; i < numberOfPasteboardItems; ++i) {
        RefPtr<DocumentFragment> fragment = client()->documentFragmentFromDelegate(i);
        if (!fragment)
            continue;

        reader.addFragment(WTFMove(fragment));
    }

    RefPtr<DocumentFragment> fragment = reader.fragment;
    if (!fragment) {
        bool chosePlainTextIgnored;
        fragment = webContentFromPasteboard(*pasteboard, *range, allowPlainText, chosePlainTextIgnored);
    }

    if (fragment && shouldInsertFragment(fragment, range, EditorInsertActionPasted))
        pasteAsFragment(fragment.releaseNonNull(), canSmartReplaceWithPasteboard(*pasteboard), false, mailBlockquoteHandling);
}

RefPtr<DocumentFragment> Editor::createFragmentAndAddResources(NSAttributedString *string)
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
        for (auto& resource : resources) {
            if (resource)
                loader->addArchiveResource(resource.releaseNonNull());
        }
    }

    if (!wasDeferringCallbacks)
        m_frame.page()->setDefersLoading(false);
    
    return fragment;
}

RefPtr<DocumentFragment> Editor::createFragmentForImageResourceAndAddResource(RefPtr<ArchiveResource>&& resource)
{
    if (!resource)
        return nullptr;

    Ref<Element> imageElement = m_frame.document()->createElement(HTMLNames::imgTag, false);

    NSURL *URL = resource->url();
    imageElement->setAttribute(HTMLNames::srcAttr, [URL isFileURL] ? [URL absoluteString] : resource->url());

    // FIXME: The code in createFragmentAndAddResources calls setDefersLoading(true). Don't we need that here?
    if (DocumentLoader* loader = m_frame.loader().documentLoader())
        loader->addArchiveResource(resource.releaseNonNull());

    auto fragment = m_frame.document()->createDocumentFragment();
    fragment->appendChild(WTFMove(imageElement));

    return WTFMove(fragment);
}

void Editor::replaceSelectionWithAttributedString(NSAttributedString *attributedString, MailBlockquoteHandling mailBlockquoteHandling)
{
    if (m_frame.selection().isNone())
        return;

    if (m_frame.selection().selection().isContentRichlyEditable()) {
        RefPtr<DocumentFragment> fragment = createFragmentAndAddResources(attributedString);
        if (fragment && shouldInsertFragment(fragment, selectedRange(), EditorInsertActionPasted))
            pasteAsFragment(fragment.releaseNonNull(), false, false, mailBlockquoteHandling);
    } else {
        String text = [attributedString string];
        if (shouldInsertText(text, selectedRange().get(), EditorInsertActionPasted))
            pasteAsPlainText(text, false);
    }
}

} // namespace WebCore
