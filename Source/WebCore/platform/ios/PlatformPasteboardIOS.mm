/*
 * Copyright (C) 2006-2013 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "Color.h"
#import "KURL.h"
#import "Image.h"
#import "PlatformPasteboard.h"
#import "SoftLinking.h"

SOFT_LINK_FRAMEWORK(UIKit)
SOFT_LINK_CLASS(UIKit, UIPasteboard)

@interface UIPasteboard
+ (UIPasteboard *)generalPasteboard;
- (void)setItems:(NSArray *)items;
@end

// FIXME: the following soft linking and #define needs to be shared
// with PasteboardIOS.mm.
SOFT_LINK_FRAMEWORK(MobileCoreServices)

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

namespace WebCore {

PlatformPasteboard::PlatformPasteboard()
    : m_pasteboard([getUIPasteboardClass() generalPasteboard])
{
}

PlatformPasteboard::PlatformPasteboard(const String&)
    : m_pasteboard([getUIPasteboardClass() generalPasteboard])
{
}

void PlatformPasteboard::getTypes(Vector<String>&)
{
}

PassRefPtr<SharedBuffer> PlatformPasteboard::bufferForType(const String&)
{
    return nullptr;
}

void PlatformPasteboard::getPathnamesForType(Vector<String>&, const String&)
{
}

String PlatformPasteboard::stringForType(const String&)
{
    return String();
}

Color PlatformPasteboard::color()
{
    return Color();
}

KURL PlatformPasteboard::url()
{
    return KURL();
}

long PlatformPasteboard::copy(const String&)
{
    return 0;
}

long PlatformPasteboard::addTypes(const Vector<String>&)
{
    return 0;
}

long PlatformPasteboard::setTypes(const Vector<String>&)
{
    return 0;
}

long PlatformPasteboard::setBufferForType(PassRefPtr<SharedBuffer>, const String&)
{
    return 0;
}

long PlatformPasteboard::setPathnamesForType(const Vector<String>&, const String&)
{
    return 0;
}

long PlatformPasteboard::setStringForType(const String&, const String&)
{
    return 0;
}

long PlatformPasteboard::changeCount() const
{
    return 0;
}

String PlatformPasteboard::uniqueName()
{
    return String();
}

void PlatformPasteboard::write(const PasteboardWebContent& content)
{
    RetainPtr<NSDictionary> representations = adoptNS([[NSMutableDictionary alloc] init]);

    if (content.dataInWebArchiveFormat)
        [representations setValue:(NSData *)content.dataInWebArchiveFormat->createNSData() forKey:WebArchivePboardType];

    if (content.dataInRTFDFormat)
        [representations setValue:content.dataInRTFDFormat->createNSData() forKey:(NSString *)kUTTypeRTFD];
    if (content.dataInRTFFormat)
        [representations setValue:content.dataInRTFFormat->createNSData() forKey:(NSString *)kUTTypeRTF];
    [representations setValue:content.dataInStringFormat forKey:(NSString *)kUTTypeText];
    [m_pasteboard setItems:[NSArray arrayWithObject:representations.get()]];
}

void PlatformPasteboard::write(const PasteboardImage& pasteboardImage)
{
    RetainPtr<NSMutableDictionary> representations = adoptNS([[NSMutableDictionary alloc] init]);
    if (!pasteboardImage.resourceMIMEType.isNull()) {
        [representations setObject:pasteboardImage.image->data()->createNSData() forKey:pasteboardImage.resourceMIMEType];
        [representations setObject:(NSString *)pasteboardImage.url.url forKey:(NSString *)kUTTypeURL];
    }
    [m_pasteboard setItems:[NSArray arrayWithObject:representations.get()]];
}

void PlatformPasteboard::write(const String& text)
{
    RetainPtr<NSDictionary> representations = adoptNS([[NSMutableDictionary alloc] init]);
    [representations setValue:text forKey:(NSString *)kUTTypeText];
    [m_pasteboard setItems:[NSArray arrayWithObject:representations.get()]];
}

}
