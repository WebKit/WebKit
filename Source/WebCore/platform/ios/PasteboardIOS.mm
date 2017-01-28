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
#import "Document.h"
#import "DocumentFragment.h"
#import "DocumentLoader.h"
#import "Editor.h"
#import "EditorClient.h"
#import "Frame.h"
#import "FrameLoader.h"
#import "FrameLoaderClient.h"
#import "HTMLElement.h"
#import "HTMLParserIdioms.h"
#import "Image.h"
#import "LegacyWebArchive.h"
#import "PasteboardStrategy.h"
#import "PlatformStrategies.h"
#import "RenderImage.h"
#import "RuntimeApplicationChecks.h"
#import "SharedBuffer.h"
#import "Text.h"
#import "URL.h"
#import "WebNSAttributedStringExtras.h"
#import "htmlediting.h"
#import "markup.h"
#import <MobileCoreServices/MobileCoreServices.h>

@interface NSAttributedString (NSAttributedStringKitAdditions)
- (id)initWithRTF:(NSData *)data documentAttributes:(NSDictionary **)dict;
- (id)initWithRTFD:(NSData *)data documentAttributes:(NSDictionary **)dict;
- (NSData *)RTFFromRange:(NSRange)range documentAttributes:(NSDictionary *)dict;
- (NSData *)RTFDFromRange:(NSRange)range documentAttributes:(NSDictionary *)dict;
- (BOOL)containsAttachments;
@end

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/PasteboardAdditions.mm>
#endif

namespace WebCore {

// FIXME: Does this need to be declared in the header file?
WEBCORE_EXPORT NSString *WebArchivePboardType = @"Apple Web Archive pasteboard type";

// Making this non-inline so that WebKit 2's decoding doesn't have to include SharedBuffer.h.
PasteboardWebContent::PasteboardWebContent()
{
}

PasteboardWebContent::~PasteboardWebContent()
{
}
    
// Making this non-inline so that WebKit 2's decoding doesn't have to include Image.h.
PasteboardImage::PasteboardImage()
{
}

PasteboardImage::~PasteboardImage()
{
}

Pasteboard::Pasteboard()
    : m_changeCount(platformStrategies()->pasteboardStrategy()->changeCount(m_pasteboardName))
{
}

void Pasteboard::writeMarkup(const String&)
{
}

std::unique_ptr<Pasteboard> Pasteboard::createForCopyAndPaste()
{
    return std::make_unique<Pasteboard>();
}

std::unique_ptr<Pasteboard> Pasteboard::createPrivate()
{
    return std::make_unique<Pasteboard>();
}

void Pasteboard::write(const PasteboardWebContent& content)
{
    platformStrategies()->pasteboardStrategy()->writeToPasteboard(content, m_pasteboardName);
}

String Pasteboard::resourceMIMEType(const NSString *mimeType)
{
    return String(adoptCF(UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType, (CFStringRef)mimeType, NULL)).get());
}

void Pasteboard::write(const PasteboardImage& pasteboardImage)
{
    platformStrategies()->pasteboardStrategy()->writeToPasteboard(pasteboardImage, m_pasteboardName);
}

void Pasteboard::writePlainText(const String& text, SmartReplaceOption)
{
    platformStrategies()->pasteboardStrategy()->writeToPasteboard(kUTTypeText, text, m_pasteboardName);
}

void Pasteboard::write(const PasteboardURL& pasteboardURL)
{
    platformStrategies()->pasteboardStrategy()->writeToPasteboard(kUTTypeURL, pasteboardURL.url.string(), m_pasteboardName);
}

void Pasteboard::writeTrustworthyWebURLsPboardType(const PasteboardURL&)
{
    // A trustworthy URL pasteboard type needs to be decided on
    // before we allow calls to this function. A page data transfer
    // should not use the same pasteboard type as this function for
    // URLs.
    ASSERT_NOT_REACHED();
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
    PasteboardStrategy& strategy = *platformStrategies()->pasteboardStrategy();
    text.text = strategy.readStringFromPasteboard(0, kUTTypeURL, m_pasteboardName);
    if (!text.text.isNull() && !text.text.isEmpty()) {
        text.isURL = true;
        return;
    }

    text.text = strategy.readStringFromPasteboard(0, kUTTypeText, m_pasteboardName);
    text.isURL = false;
}

static NSArray* supportedImageTypes()
{
    return @[(id)kUTTypePNG, (id)kUTTypeTIFF, (id)kUTTypeJPEG, (id)kUTTypeGIF];
}

void Pasteboard::read(PasteboardWebContentReader& reader)
{
    PasteboardStrategy& strategy = *platformStrategies()->pasteboardStrategy();

    int numberOfItems = strategy.getPasteboardItemsCount(m_pasteboardName);

    if (!numberOfItems)
        return;

    NSArray *types = supportedPasteboardTypes();
    int numberOfTypes = [types count];

    for (int i = 0; i < numberOfItems; i++) {
        for (int typeIndex = 0; typeIndex < numberOfTypes; typeIndex++) {
            NSString *type = [types objectAtIndex:typeIndex];

            if ([type isEqualToString:WebArchivePboardType]) {
                if (RefPtr<SharedBuffer> buffer = strategy.readBufferFromPasteboard(i, WebArchivePboardType, m_pasteboardName)) {
                    if (reader.readWebArchive(buffer.get()))
                        break;
                }
            }

            if ([type isEqualToString:(NSString *)kUTTypeHTML]) {
                String htmlString = strategy.readStringFromPasteboard(i, kUTTypeHTML, m_pasteboardName);
                if (!htmlString.isNull() && reader.readHTML(htmlString))
                    break;
            }

            if ([type isEqualToString:(NSString *)kUTTypeFlatRTFD]) {
                if (RefPtr<SharedBuffer> buffer = strategy.readBufferFromPasteboard(i, kUTTypeFlatRTFD, m_pasteboardName)) {
                    if (reader.readRTFD(*buffer))
                        break;
                }
            }

            if ([type isEqualToString:(NSString *)kUTTypeRTF]) {
                if (RefPtr<SharedBuffer> buffer = strategy.readBufferFromPasteboard(i, kUTTypeRTF, m_pasteboardName)) {
                    if (reader.readRTF(*buffer))
                        break;
                }
            }

            if ([supportedImageTypes() containsObject:type]) {
                if (RefPtr<SharedBuffer> buffer = strategy.readBufferFromPasteboard(i, type, m_pasteboardName)) {
                    if (reader.readImage(buffer.releaseNonNull(), type))
                        break;
                }
        }

            if ([type isEqualToString:(NSString *)kUTTypeURL]) {
                URL url = strategy.readURLFromPasteboard(i, kUTTypeURL, m_pasteboardName);
                if (!url.isNull() && reader.readURL(url, String()))
                    break;
            }
            
            if ([type isEqualToString:(NSString *)kUTTypeText]) {
                String string = strategy.readStringFromPasteboard(i, kUTTypeText, m_pasteboardName);
                if (!string.isNull() && reader.readPlainText(string))
                    break;
            }

        }
    }
}

NSArray* Pasteboard::supportedPasteboardTypes()
{
    return @[(id)WebArchivePboardType, (id)kUTTypeFlatRTFD, (id)kUTTypeRTF, (id)kUTTypeHTML, (id)kUTTypePNG, (id)kUTTypeTIFF, (id)kUTTypeJPEG, (id)kUTTypeGIF, (id)kUTTypeURL, (id)kUTTypeText];
}

bool Pasteboard::hasData()
{
    return !!platformStrategies()->pasteboardStrategy()->getPasteboardItemsCount(m_pasteboardName);
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

    platformStrategies()->pasteboardStrategy()->writeToPasteboard(cocoaType.get(), String(), m_pasteboardName);
}

void Pasteboard::clear()
{
    platformStrategies()->pasteboardStrategy()->writeToPasteboard(String(), String(), m_pasteboardName);
}

String Pasteboard::readString(const String& type)
{
    PasteboardStrategy& strategy = *platformStrategies()->pasteboardStrategy();

    int numberOfItems = strategy.getPasteboardItemsCount(m_pasteboardName);

    if (!numberOfItems)
        return String();

    // Grab the value off the pasteboard corresponding to the cocoaType.
    RetainPtr<NSString> cocoaType = cocoaTypeFromHTMLClipboardType(type);

    NSString *cocoaValue = nil;

    if ([cocoaType isEqualToString:(NSString *)kUTTypeURL]) {
        URL url = strategy.readURLFromPasteboard(0, kUTTypeURL, m_pasteboardName);
        if (!url.isNull())
            cocoaValue = [(NSURL *)url absoluteString];
    } else if ([cocoaType isEqualToString:(NSString *)kUTTypeText]) {
        String value = strategy.readStringFromPasteboard(0, kUTTypeText, m_pasteboardName);
        if (!value.isNull())
            cocoaValue = [(NSString *)value precomposedStringWithCanonicalMapping];
    } else if (cocoaType) {
        if (RefPtr<SharedBuffer> buffer = strategy.readBufferFromPasteboard(0, cocoaType.get(), m_pasteboardName))
            cocoaValue = [[[NSString alloc] initWithData:buffer->createNSData().get() encoding:NSUTF8StringEncoding] autorelease];
    }

    // Enforce changeCount ourselves for security. We check after reading instead of before to be
    // sure it doesn't change between our testing the change count and accessing the data.
    if (cocoaValue && m_changeCount == platformStrategies()->pasteboardStrategy()->changeCount(m_pasteboardName))
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

void Pasteboard::writeString(const String& type, const String& data)
{
    RetainPtr<NSString> cocoaType = cocoaTypeFromHTMLClipboardType(type);
    if (!cocoaType)
        return;

    platformStrategies()->pasteboardStrategy()->writeToPasteboard(type, data, m_pasteboardName);
}

Vector<String> Pasteboard::types()
{
    NSArray* types = supportedPasteboardTypes();

    // Enforce changeCount ourselves for security. We check after reading instead of before to be
    // sure it doesn't change between our testing the change count and accessing the data.
    if (m_changeCount != platformStrategies()->pasteboardStrategy()->changeCount(m_pasteboardName))
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
