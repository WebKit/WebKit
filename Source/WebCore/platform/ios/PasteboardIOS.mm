/*
 * Copyright (C) 2007, 2008, 2012, 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "Pasteboard.h"

#import "CachedImage.h"
#import "DOMRangeInternal.h"
#import "Document.h"
#import "DocumentFragment.h"
#import "DocumentLoader.h"
#import "Editor.h"
#import "EditorClient.h"
#import "Frame.h"
#import "FrameLoader.h"
#import "FrameLoaderClient.h"
#import "HTMLElement.h"
#import "HTMLNames.h"
#import "HTMLParserIdioms.h"
#import "KURL.h"
#import "LegacyWebArchive.h"
#import "Page.h"
#import "RenderImage.h"
#import "SoftLinking.h"
#import "Text.h"
#import "htmlediting.h"
#import "markup.h"
#import "WebNSAttributedStringExtras.h"
#import <MobileCoreServices/MobileCoreServices.h>

@interface NSHTMLReader
- (id)initWithDOMRange:(DOMRange *)domRange;
- (NSAttributedString *)attributedString;
@end

@interface NSAttributedString (NSAttributedStringKitAdditions)
- (id)initWithRTF:(NSData *)data documentAttributes:(NSDictionary **)dict;
- (id)initWithRTFD:(NSData *)data documentAttributes:(NSDictionary **)dict;
- (NSData *)RTFFromRange:(NSRange)range documentAttributes:(NSDictionary *)dict;
- (NSData *)RTFDFromRange:(NSRange)range documentAttributes:(NSDictionary *)dict;
- (BOOL)containsAttachments;
@end

SOFT_LINK_PRIVATE_FRAMEWORK(UIFoundation)
SOFT_LINK_CLASS(UIFoundation, NSHTMLReader)

SOFT_LINK_FRAMEWORK(MobileCoreServices)

SOFT_LINK(MobileCoreServices, UTTypeConformsTo, Boolean, (CFStringRef inUTI, CFStringRef inConformsToUTI), (inUTI, inConformsToUTI))
SOFT_LINK(MobileCoreServices, UTTypeCreatePreferredIdentifierForTag, CFStringRef, (CFStringRef inTagClass, CFStringRef inTag, CFStringRef inConformingToUTI), (inTagClass, inTag, inConformingToUTI))
SOFT_LINK(MobileCoreServices, UTTypeCopyPreferredTagWithClass, CFStringRef, (CFStringRef inUTI, CFStringRef inTagClass), (inUTI, inTagClass))

SOFT_LINK_CONSTANT(MobileCoreServices, kUTTypeText, CFStringRef)
SOFT_LINK_CONSTANT(MobileCoreServices, kUTTypePNG, CFStringRef)
SOFT_LINK_CONSTANT(MobileCoreServices, kUTTypeJPEG, CFStringRef)
SOFT_LINK_CONSTANT(MobileCoreServices, kUTTypeURL, CFStringRef)
SOFT_LINK_CONSTANT(MobileCoreServices, kUTTypeTIFF, CFStringRef)
SOFT_LINK_CONSTANT(MobileCoreServices, kUTTypeGIF, CFStringRef)
SOFT_LINK_CONSTANT(MobileCoreServices, kUTTagClassMIMEType, CFStringRef)
SOFT_LINK_CONSTANT(MobileCoreServices, kUTTagClassFilenameExtension, CFStringRef)
SOFT_LINK_CONSTANT(MobileCoreServices, kUTTypeRTFD, CFStringRef)
SOFT_LINK_CONSTANT(MobileCoreServices, kUTTypeRTF, CFStringRef)

#define kUTTypeText getkUTTypeText()
#define kUTTypePNG  getkUTTypePNG()
#define kUTTypeJPEG getkUTTypeJPEG()
#define kUTTypeURL  getkUTTypeURL()
#define kUTTypeTIFF getkUTTypeTIFF()
#define kUTTypeGIF  getkUTTypeGIF()
#define kUTTagClassMIMEType getkUTTagClassMIMEType()
#define kUTTagClassFilenameExtension getkUTTagClassFilenameExtension()
#define kUTTypeRTFD getkUTTypeRTFD()
#define kUTTypeRTF getkUTTypeRTF()

SOFT_LINK_FRAMEWORK(AppSupport)
SOFT_LINK(AppSupport, CPSharedResourcesDirectory, CFStringRef, (void), ())

namespace WebCore {

// FIXME: Does this need to be declared in the header file?
NSString *WebArchivePboardType = @"Apple Web Archive pasteboard type";

Pasteboard::Pasteboard()
    : m_frame(0)
{
}

PassOwnPtr<Pasteboard> Pasteboard::createForCopyAndPaste()
{
    return adoptPtr(new Pasteboard);
}

PassOwnPtr<Pasteboard> Pasteboard::createPrivate()
{
    return adoptPtr(new Pasteboard);
}

void Pasteboard::setFrame(Frame* frame)
{
    m_frame = frame;
    m_changeCount = m_frame->editor().client()->pasteboardChangeCount();
}

void Pasteboard::writeSelection(Range* selectedRange, bool /*canSmartCopyOrDelete*/, Frame *frame, ShouldSerializeSelectedTextForClipboard shouldSerializeSelectedTextForClipboard)
{
    ASSERT(selectedRange);
    ASSERT(frame);

    // If the selection is at the beginning of content inside an anchor tag
    // we move the selection start to include the anchor.
    ExceptionCode ec;
    Node* commonAncestor = selectedRange->commonAncestorContainer(ec);
    ASSERT(commonAncestor);
    Node* enclosingAnchor = enclosingNodeWithTag(firstPositionInNode(commonAncestor), HTMLNames::aTag);
    if (enclosingAnchor && comparePositions(firstPositionInOrBeforeNode(selectedRange->startPosition().anchorNode()), selectedRange->startPosition()) >= 0)
        selectedRange->setStart(enclosingAnchor, 0, ec);

    frame->editor().client()->didSetSelectionTypesForPasteboard();

    RetainPtr<NSDictionary> representations = adoptNS([[NSMutableDictionary alloc] init]);

    // Put WebArchive on the pasteboard.
    RefPtr<LegacyWebArchive> archive = LegacyWebArchive::createFromSelection(frame);
    RetainPtr<CFDataRef> data = archive ? archive->rawDataRepresentation() : 0;
    if (data)
        [representations.get() setValue:(NSData *)data.get() forKey:WebArchivePboardType];

    RetainPtr<NSHTMLReader> converter = adoptNS([[getNSHTMLReaderClass() alloc] initWithDOMRange:kit(selectedRange)]);
    if (converter) {
        NSAttributedString *attributedString = [converter.get() attributedString];
        NSData* RTFDData = [attributedString RTFDFromRange:NSMakeRange(0, [attributedString length]) documentAttributes:nil];
        if (RTFDData)
            [representations.get() setValue:RTFDData forKey:(NSString *)kUTTypeRTFD];
        if ([attributedString containsAttachments])
            attributedString = attributedStringByStrippingAttachmentCharacters(attributedString);
        NSData* RTFData = [attributedString RTFFromRange:NSMakeRange(0, [attributedString length]) documentAttributes:nil];
        if (RTFData)
            [representations.get() setValue:RTFData forKey:(NSString *)kUTTypeRTF];
    }

    // Put plain string on the pasteboard.
    String text = shouldSerializeSelectedTextForClipboard == IncludeImageAltTextForClipboard
        ? frame->editor().selectedTextForClipboard() : frame->editor().selectedText();
    text.replace(noBreakSpace, ' ');
    [representations.get() setValue:text forKey:(NSString *)kUTTypeText];

    frame->editor().client()->writeDataToPasteboard(representations.get());
}

void Pasteboard::writePlainText(const String& text, Frame *frame)
{
    ASSERT(frame);

    RetainPtr<NSDictionary> representations = adoptNS([[NSMutableDictionary alloc] init]);
    [representations.get() setValue:text forKey:(NSString *)kUTTypeText];
    frame->editor().client()->writeDataToPasteboard(representations.get());
}

void Pasteboard::writeImage(Node* node, Frame* frame)
{
    ASSERT(node);

    if (!(node->renderer() && node->renderer()->isImage()))
        return;

    RenderImage* renderer = toRenderImage(node->renderer());
    CachedImage* cachedImage = renderer->cachedImage();
    if (!cachedImage || cachedImage->errorOccurred())
        return;

    Image* image = cachedImage->imageForRenderer(renderer);
    ASSERT(image);

    RetainPtr<NSData> imageData = image->data()->createNSData();

    if (!imageData)
        return;

    RetainPtr<NSMutableDictionary> dictionary = adoptNS([[NSMutableDictionary alloc] init]);
    NSString *mimeType = cachedImage->response().mimeType();
    RetainPtr<CFStringRef> uti = adoptCF(UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType, (CFStringRef)mimeType, NULL));
    if (uti) {
        [dictionary.get() setObject:imageData.get() forKey:(NSString *)uti.get()];
        [dictionary.get() setObject:(NSString *)node->document().completeURL(stripLeadingAndTrailingHTMLSpaces(static_cast<HTMLElement*>(node)->getAttribute("src"))) forKey:(NSString *)kUTTypeURL];
    }
    frame->editor().client()->writeDataToPasteboard(dictionary.get());
}

void Pasteboard::writePlainText(const String&, SmartReplaceOption)
{
}

void Pasteboard::writePasteboard(const Pasteboard&)
{
}

bool Pasteboard::canSmartReplace()
{
    return false;
}

void Pasteboard::read(PasteboardPlainText& text)
{
    RetainPtr<NSArray> pasteboardItem = m_frame->editor().client()->readDataFromPasteboard((NSString *)kUTTypeText, 0);

    if ([pasteboardItem.get() count] == 0)
        return;

    id value = [pasteboardItem.get() objectAtIndex:0];
    ASSERT([value isKindOfClass:[NSString class]]);
    if ([value isKindOfClass:[NSString class]])
        text.text = (NSString *)value;
}

static NSArray* supportedImageTypes()
{
    return [NSArray arrayWithObjects:(id)kUTTypePNG, (id)kUTTypeTIFF, (id)kUTTypeJPEG, (id)kUTTypeGIF, nil];
}

NSArray* Pasteboard::supportedPasteboardTypes()
{
    return [NSArray arrayWithObjects:(id)WebArchivePboardType, (id)kUTTypePNG, (id)kUTTypeTIFF, (id)kUTTypeJPEG, (id)kUTTypeGIF, (id)kUTTypeURL, (id)kUTTypeText, (id)kUTTypeRTFD, (id)kUTTypeRTF, nil];
}

#define WebDataProtocolScheme @"webkit-fake-url"

static NSURL* uniqueURLWithRelativePart(NSString *relativePart)
{
    CFUUIDRef UUIDRef = CFUUIDCreate(kCFAllocatorDefault);
    NSString *UUIDString = (NSString *)CFUUIDCreateString(kCFAllocatorDefault, UUIDRef);
    CFRelease(UUIDRef);
    NSURL *URL = [NSURL URLWithString:[NSString stringWithFormat:@"%@://%@/%@", WebDataProtocolScheme, UUIDString, relativePart]];
    CFRelease(UUIDString);

    return URL;
}

static PassRefPtr<DocumentFragment> documentFragmentWithImageResource(Frame* frame, PassRefPtr<ArchiveResource> resource)
{
    RefPtr<Element> imageElement = frame->document()->createElement(HTMLNames::imgTag, false);

    if (DocumentLoader* loader = frame->loader().documentLoader())
        loader->addArchiveResource(resource.get());

    NSURL *URL = resource->url();
    imageElement->setAttribute(HTMLNames::srcAttr, [URL isFileURL] ? [URL absoluteString] : resource->url());
    RefPtr<DocumentFragment> fragment = frame->document()->createDocumentFragment();
    fragment->appendChild(imageElement.release());
    return fragment.release();
}

static PassRefPtr<DocumentFragment> documentFragmentWithLink(Document* document, const String& urlString)
{
    RefPtr<Element> anchorElement = document->createElement(HTMLNames::aTag, false);

    anchorElement->setAttribute(HTMLNames::hrefAttr, urlString);
    anchorElement->appendChild(document->createTextNode(urlString));

    RefPtr<DocumentFragment> fragment = document->createDocumentFragment();
    fragment->appendChild(anchorElement.release());
    return fragment.release();
}

static PassRefPtr<DocumentFragment> documentFragmentWithRTF(Frame* frame, NSString *pasteboardType, NSData* pasteboardData)
{
    if (!frame || !frame->document() || !frame->document()->isHTMLDocument())
        return 0;

    RetainPtr<NSAttributedString> string;
    if ([pasteboardType isEqualToString:(NSString *)kUTTypeRTFD])
        string = [[NSAttributedString alloc] initWithRTFD:pasteboardData documentAttributes:NULL];

    if (!string)
        string = [[NSAttributedString alloc] initWithRTF:pasteboardData documentAttributes:NULL];

    if (!string)
        return 0;

    bool wasDeferringCallbacks = frame->page()->defersLoading();
    if (!wasDeferringCallbacks)
        frame->page()->setDefersLoading(true);

    Vector<RefPtr<ArchiveResource> > resources;
    RefPtr<DocumentFragment> fragment = frame->editor().client()->documentFragmentFromAttributedString(string.get(), resources);

    size_t size = resources.size();
    if (size) {
        DocumentLoader* loader = frame->loader().documentLoader();
        for (size_t i = 0; i < size; ++i)
            loader->addArchiveResource(resources[i]);
    }

    if (!wasDeferringCallbacks)
        frame->page()->setDefersLoading(false);

    return fragment.release();
}

PassRefPtr<DocumentFragment> Pasteboard::documentFragmentForPasteboardItemAtIndex(Frame* frame, int index, bool allowPlainText, bool& chosePlainText)
{
    RefPtr<DocumentFragment> fragment = frame->editor().client()->documentFragmentFromDelegate(index);
    if (fragment)
        return fragment.release();

    // First try to ask the client about the supported types. It will return null if the client
    // has no selection.
    NSArray *supportedTypes = frame->editor().client()->supportedPasteboardTypesForCurrentSelection();
    if (!supportedTypes)
        supportedTypes = supportedPasteboardTypes();
    int numberOfTypes = [supportedTypes count];

    for (int i = 0; i < numberOfTypes; i++) {
        NSString *type = [supportedTypes objectAtIndex:i];
        RetainPtr<NSArray> pasteboardItem = frame->editor().client()->readDataFromPasteboard(type, index);

        if ([pasteboardItem.get() count] == 0)
            continue;

        if ([type isEqualToString:WebArchivePboardType]) {
            if (!frame->document())
                return 0;

            // We put [WebArchive data] on the pasteboard in -copy: instead of the archive itself until there is API to provide the WebArchive.
            NSData *data = [pasteboardItem.get() objectAtIndex:0];
            RefPtr<LegacyWebArchive> coreArchive = LegacyWebArchive::create(SharedBuffer::wrapNSData(data).get());
            if (coreArchive) {
                RefPtr<ArchiveResource> mainResource = coreArchive->mainResource();
                if (mainResource) {
                    NSString *MIMEType = mainResource->mimeType();
                    if (frame->loader().client().canShowMIMETypeAsHTML(MIMEType)) {
                        RetainPtr<NSString> markupString = adoptNS([[NSString alloc] initWithData:[mainResource->data()->createNSData() autorelease] encoding:NSUTF8StringEncoding]);
                        if (DocumentLoader* loader = frame->loader().documentLoader())
                            loader->addAllArchiveResources(coreArchive.get());

                        fragment = createFragmentFromMarkup(frame->document(), markupString.get(), mainResource->url(), DisallowScriptingContent);
                    }
                }
                if (fragment)
                    return fragment.release();
            }
        }

        if ([type isEqualToString:(NSString *)kUTTypeRTFD])
            return documentFragmentWithRTF(frame, (NSString *)kUTTypeRTFD, [pasteboardItem.get() objectAtIndex:0]);

        if ([type isEqualToString:(NSString *)kUTTypeRTF])
            return documentFragmentWithRTF(frame, (NSString *)kUTTypeRTF, [pasteboardItem.get() objectAtIndex:0]);

        if ([supportedImageTypes() containsObject:type]) {
            RetainPtr<NSString> filenameExtension = adoptNS((NSString *)UTTypeCopyPreferredTagWithClass((CFStringRef)type, kUTTagClassFilenameExtension));
            NSString *relativeURLPart = [@"image" stringByAppendingString:filenameExtension.get()];
            RetainPtr<NSString> mimeType = adoptNS((NSString *)UTTypeCopyPreferredTagWithClass((CFStringRef)type, kUTTagClassMIMEType));
            NSData *data = [pasteboardItem.get() objectAtIndex:0];
            return documentFragmentWithImageResource(frame, ArchiveResource::create(SharedBuffer::wrapNSData([[data copy] autorelease]), uniqueURLWithRelativePart(relativeURLPart), mimeType.get(), "", ""));
        }
        if ([type isEqualToString:(NSString *)kUTTypeURL]) {
            id value = [pasteboardItem.get() objectAtIndex:0];
            if (![value isKindOfClass:[NSURL class]]) {
                ASSERT([value isKindOfClass:[NSURL class]]);
                return 0;
            }
            NSURL *url = (NSURL *)value;

            if (!frame->editor().client()->hasRichlyEditableSelection()) {
                fragment = createFragmentFromText(frame->selection().toNormalizedRange().get(), [url absoluteString]);
                if (fragment)
                    return fragment.release();
            }

            if ([url isFileURL]) {
                NSString *localPath = [url relativePath];
                // Only allow url attachments from ~/Media for now.
                if (![localPath hasPrefix:[(NSString *)CPSharedResourcesDirectory() stringByAppendingString:@"/Media/DCIM/"]])
                    continue;

                RetainPtr<NSString> fileType = adoptNS((NSString *)UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, (CFStringRef)[localPath pathExtension], NULL));
                NSData *data = [NSData dataWithContentsOfFile:localPath];
                if (UTTypeConformsTo((CFStringRef)fileType.get(), kUTTypePNG))
                    return documentFragmentWithImageResource(frame, ArchiveResource::create(SharedBuffer::wrapNSData([[data copy] autorelease]), uniqueURLWithRelativePart(@"image.png"), @"image/png", "", ""));
                else if (UTTypeConformsTo((CFStringRef)fileType.get(), kUTTypeJPEG))
                    return documentFragmentWithImageResource(frame, ArchiveResource::create(SharedBuffer::wrapNSData([[data copy] autorelease]), uniqueURLWithRelativePart(@"image.jpg"), @"image/jpg", "", ""));
            } else {
                // Create a link with URL text.
                return documentFragmentWithLink(frame->document(), [url absoluteString]);
            }
        }
        if (allowPlainText && [type isEqualToString:(NSString *)kUTTypeText]) {
            id value = [pasteboardItem.get() objectAtIndex:0];
            if (![value isKindOfClass:[NSString class]]) {
                ASSERT([value isKindOfClass:[NSString class]]);
                return 0;
            }

            chosePlainText = true;
            fragment = createFragmentFromText(frame->selection().toNormalizedRange().get(), (NSString *)value);
            if (fragment)
                return fragment.release();
        }
    }

    return 0;
}

PassRefPtr<DocumentFragment> Pasteboard::documentFragment(Frame* frame, PassRefPtr<Range> /*context*/, bool allowPlainText, bool& chosePlainText)
{
    chosePlainText = false;

    if (!frame)
        return 0;

    int numberOfItems = frame->editor().client()->getPasteboardItemsCount();

    if (!numberOfItems)
        return 0;

    // In the common case there is just one item on the pasteboard, avoid the expense of transferring the content of
    // fragmentForCurrentItem to the main fragment.
    RefPtr<DocumentFragment> fragment = documentFragmentForPasteboardItemAtIndex(frame, 0, allowPlainText, chosePlainText);

    for (int i = 1; i < numberOfItems; i++) {
        RefPtr<DocumentFragment> fragmentForCurrentItem = documentFragmentForPasteboardItemAtIndex(frame, i, allowPlainText, chosePlainText);
        if (!fragment)
            fragment = fragmentForCurrentItem;
        else if (fragmentForCurrentItem && fragmentForCurrentItem->firstChild()) {
            ExceptionCode ec;
            fragment->appendChild(fragmentForCurrentItem->firstChild(), ec);
        }
    }

    if (fragment)
        return fragment.release();

    return 0;
}

bool Pasteboard::hasData()
{
    return m_frame->editor().client()->getPasteboardItemsCount() != 0;
}

static String utiTypeFromCocoaType(NSString *type)
{
    RetainPtr<CFStringRef> utiType = adoptCF(UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType, (CFStringRef)type, NULL));
    if (!utiType)
        return String();
    return String(adoptCF(UTTypeCopyPreferredTagWithClass(utiType.get(), kUTTagClassMIMEType)).get());
}

static RetainPtr<NSString> cocoaTypeFromHTMLClipboardType(const String& type)
{
    String strippedType = type.stripWhiteSpace();

    if (strippedType == "Text")
        return (NSString *)kUTTypeText;
    if (strippedType == "URL")
        return (NSString *)kUTTypeURL;

    // Ignore any trailing charset - JS strings are Unicode, which encapsulates the charset issue.
    if (strippedType.startsWith("text/plain"))
        return (NSString *)kUTTypeText;

    // Special case because UTI doesn't work with Cocoa's URL type.
    if (strippedType == "text/uri-list")
        return (NSString *)kUTTypeURL;

    // Try UTI now.
    if (NSString *utiType = utiTypeFromCocoaType(strippedType))
        return utiType;

    // No mapping, just pass the whole string though.
    return (NSString *)strippedType;
}

void Pasteboard::clear(const String& type)
{
    // Since UIPasteboard enforces changeCount itself on writing, we don't check it here.

    RetainPtr<NSString> cocoaType = cocoaTypeFromHTMLClipboardType(type);
    if (!cocoaType)
        return;

    // FIXME: Should write this with @{} syntax.
    RetainPtr<NSDictionary> representations = adoptNS([[NSMutableDictionary alloc] init]);
    [representations.get() setValue:0 forKey:cocoaType.get()];
    m_frame->editor().client()->writeDataToPasteboard(representations.get());
}

void Pasteboard::clear()
{
    m_frame->editor().client()->writeDataToPasteboard(@{});
}

String Pasteboard::readString(const String& type)
{
    RetainPtr<NSString> cocoaType = cocoaTypeFromHTMLClipboardType(type);

    // Grab the value off the pasteboard corresponding to the cocoaType.
    RetainPtr<NSArray> pasteboardItem = m_frame->editor().client()->readDataFromPasteboard(cocoaType.get(), 0);

    if ([pasteboardItem.get() count] == 0)
        return String();

    NSString *cocoaValue = nil;

    if ([cocoaType.get() isEqualToString:(NSString *)kUTTypeURL]) {
        id value = [pasteboardItem.get() objectAtIndex:0];
        if (![value isKindOfClass:[NSURL class]]) {
            ASSERT([value isKindOfClass:[NSURL class]]);
            return String();
        }
        NSURL* absoluteURL = (NSURL*)value;

        if (absoluteURL)
            cocoaValue = [absoluteURL absoluteString];
    } else if ([cocoaType.get() isEqualToString:(NSString *)kUTTypeText]) {
        id value = [pasteboardItem.get() objectAtIndex:0];
        if (![value isKindOfClass:[NSString class]]) {
            ASSERT([value isKindOfClass:[NSString class]]);
            return String();
        }

        cocoaValue = [(NSString *)value precomposedStringWithCanonicalMapping];
    } else if (cocoaType) {
        ASSERT([pasteboardItem.get() count] == 1);
        id value = [pasteboardItem.get() objectAtIndex:0];
        if (![value isKindOfClass:[NSData class]]) {
            ASSERT([value isKindOfClass:[NSData class]]);
            return String();
        }
        cocoaValue = [[[NSString alloc] initWithData:(NSData *)value encoding:NSUTF8StringEncoding] autorelease];
    }

    // Enforce changeCount ourselves for security. We check after reading instead of before to be
    // sure it doesn't change between our testing the change count and accessing the data.
    if (cocoaValue && m_changeCount == m_frame->editor().client()->pasteboardChangeCount())
        return cocoaValue;

    return String();
}

static void addHTMLClipboardTypesForCocoaType(ListHashSet<String>& resultTypes, NSString *cocoaType)
{
    // UTI may not do these right, so make sure we get the right, predictable result.
    if ([cocoaType isEqualToString:(NSString *)kUTTypeText]) {
        resultTypes.add(ASCIILiteral("text/plain"));
        return;
    }
    if ([cocoaType isEqualToString:(NSString *)kUTTypeURL]) {
        resultTypes.add(ASCIILiteral("text/uri-list"));
        return;
    }
    String utiType = utiTypeFromCocoaType(cocoaType);
    if (!utiType.isEmpty()) {
        resultTypes.add(utiType);
        return;
    }
    // No mapping, just pass the whole string though.
    resultTypes.add(cocoaType);
}

bool Pasteboard::writeString(const String& type, const String& data)
{
    RetainPtr<NSString> cocoaType = cocoaTypeFromHTMLClipboardType(type);
    if (!cocoaType)
        return false;

    NSString *cocoaData = data;
    RetainPtr<NSObject> value;

    if ([cocoaType.get() isEqualToString:(NSString *)kUTTypeURL])
        value = adoptNS([[NSURL alloc] initWithString:cocoaData]);
    else {
        // Every other type we handle goes on the pasteboard as a string.
        value = cocoaData;
    }

    RetainPtr<NSDictionary> representations = adoptNS([[NSMutableDictionary alloc] init]);
    [representations.get() setValue:value.get() forKey:cocoaType.get()];
    m_frame->editor().client()->writeDataToPasteboard(representations.get());
    return true;
}

Vector<String> Pasteboard::types()
{
    NSArray* types = supportedPasteboardTypes();

    // Enforce changeCount ourselves for security. We check after reading instead of before to be
    // sure it doesn't change between our testing the change count and accessing the data.
    if (m_changeCount != m_frame->editor().client()->pasteboardChangeCount())
        return Vector<String>();

    ListHashSet<String> result;
    NSUInteger count = [types count];
    for (NSUInteger i = 0; i < count; i++) {
        NSString *type = [types objectAtIndex:i];
        addHTMLClipboardTypesForCocoaType(result, type);
    }

    Vector<String> vector;
    copyToVector(result, vector);
    return vector;
}

Vector<String> Pasteboard::readFilenames()
{
    return Vector<String>();
}

}
