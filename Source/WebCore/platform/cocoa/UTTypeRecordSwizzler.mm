/*
* Copyright (C) 2020 Apple Inc. All rights reserved.
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

#import "config.h"
#import "UTTypeRecordSwizzler.h"

#if USE(UTTYPE_SWIZZLER)

#import <MobileCoreServices/MobileCoreServices.h>

#import <WebCore/UTIUtilities.h>
#import <pal/spi/cocoa/NSUTTypeRecordSPI.h>
#import <wtf/HashSet.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/ObjCRuntimeExtras.h>
#import <wtf/SoftLinking.h>
#import <wtf/Vector.h>
#import <wtf/text/StringHash.h>
#import <wtf/text/WTFString.h>

__attribute__((visibility("default")))
@interface WebUTTypeRecord : UTTypeRecord
- (void)setIdentifier:(NSString *)i;
@end

@implementation WebUTTypeRecord {
@package
    NSString *_identifier;
}
@synthesize identifier=_identifier;
- (void)setIdentifier:(NSString *)i
{
    _identifier = i;
}
@end

namespace WebCore {

static Vector<UTTypeItem>& vectorOfUTTypeRecords()
{
    static NeverDestroyed<Vector<UTTypeItem>> vector;
    return vector;
}

static IMP typeRecordWithTagOriginal = nil;

static UTTypeRecord *typeRecordWithTagOverride(id self, SEL selector, NSString *tag, NSString *tagClass, NSString *identifier)
{
    for (auto& r : vectorOfUTTypeRecords()) {
        if (r.tag == String(tag) && r.tagClass == String(tagClass) && r.identifier == String(identifier)) {
            WebUTTypeRecord *record = [WebUTTypeRecord alloc];
            [record setIdentifier:r.uti];
            return [record autorelease];
        }
    }
    WTFLogAlways("UTI not found for tag: %s %s %s\n", String(tag).utf8().data(), String(tagClass).utf8().data(), String(identifier).utf8().data());
    return wtfCallIMP<UTTypeRecord *>(typeRecordWithTagOriginal, self, selector, tag, tagClass, identifier);
}

const Vector<UTTypeItem>& createVectorOfUTTypeItem()
{
    static NeverDestroyed<Vector<UTTypeItem>> vector = [] {
        Vector<UTTypeItem> v = std::initializer_list<UTTypeItem> {
            { "dat"_s, "public.filename-extension"_s, static_cast<char*>(nullptr), static_cast<char*>(nullptr) },
            { "mp4"_s, "public.filename-extension"_s, static_cast<char*>(nullptr), static_cast<char*>(nullptr) },
            { "otf"_s, "public.filename-extension"_s, static_cast<char*>(nullptr), static_cast<char*>(nullptr) },
        };
        for (auto& r : v) {
            auto uti = adoptCF(UTTypeCreatePreferredIdentifierForTag(r.tagClass.createCFString().get(), r.tag.createCFString().get(), r.identifier.isNull() ? nullptr : r.identifier.createCFString().get()));
            r.uti = uti.get();
        }
        for (auto& it : createUTIFromMIMETypeMap()) {
            UTTypeItem i = { it.key, "public.mime-type"_s, static_cast<char*>(nullptr), static_cast<char*>(nullptr) };
            auto uti = adoptCF(UTTypeCreatePreferredIdentifierForTag(i.tagClass.createCFString().get(), i.tag.createCFString().get(), i.identifier.isNull() ? nullptr : i.identifier.createCFString().get()));
            i.uti = uti.get();
            v.append(WTFMove(i));
        }
        return v;
    }();
    return vector;
}

void setVectorOfUTTypeItem(Vector<UTTypeItem>&& vector)
{
    vectorOfUTTypeRecords() = WTFMove(vector);
}

void swizzleUTTypeRecord()
{
    Method typeRecordWithTagMethod = class_getClassMethod(objc_getClass("UTTypeRecord"), @selector(typeRecordWithTag: ofClass: conformingToIdentifier:));
    typeRecordWithTagOriginal = method_setImplementation(typeRecordWithTagMethod, (IMP)typeRecordWithTagOverride);
}

};

#endif
