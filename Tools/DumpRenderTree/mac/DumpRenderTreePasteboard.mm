/*
 * Copyright (C) 2005, 2006, 2007 Apple, Inc.  All rights reserved.
 *           (C) 2007 Graham Dennis (graham.dennis@gmail.com)
 *           (C) 2007 Eric Seidel <eric@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "DumpRenderTreePasteboard.h"

#import "DumpRenderTreeMac.h"

#if !PLATFORM(IOS)

#import <WebKit/WebTypesInternal.h>
#import <wtf/Assertions.h>
#import <wtf/HashMap.h>
#import <wtf/ListHashSet.h>
#import <wtf/RetainPtr.h>

@interface LocalPasteboard : NSPasteboard {
    RetainPtr<NSString> _pasteboardName;
    NSInteger _changeCount;

    ListHashSet<RetainPtr<NSString>, WTF::RetainPtrObjectHash<NSString>> _types;
    HashMap<RetainPtr<NSString>, RetainPtr<NSData>, WTF::RetainPtrObjectHash<NSString>, WTF::RetainPtrObjectHashTraits<NSString>> _data;
}

-(id)initWithName:(NSString *)name;
@end

static NSMutableDictionary *localPasteboards;

@implementation DumpRenderTreePasteboard

// Return a local pasteboard so we don't disturb the real pasteboards when running tests.
+ (NSPasteboard *)_pasteboardWithName:(NSString *)name
{
    static int number = 0;
    if (!name)
        name = [NSString stringWithFormat:@"LocalPasteboard%d", ++number];
    if (!localPasteboards)
        localPasteboards = [[NSMutableDictionary alloc] init];
    LocalPasteboard *pasteboard = [localPasteboards objectForKey:name];
    if (pasteboard)
        return pasteboard;
    pasteboard = [[LocalPasteboard alloc] initWithName:name];
    [localPasteboards setObject:pasteboard forKey:name];
    [pasteboard release];
    return pasteboard;
}

+ (void)releaseLocalPasteboards
{
    [localPasteboards release];
    localPasteboards = nil;
}

// Convenience method for JS so that it doesn't have to try and create a NSArray on the objc side instead
// of the usual WebScriptObject that is passed around
- (NSInteger)declareType:(NSString *)type owner:(id)newOwner
{
    return [self declareTypes:[NSArray arrayWithObject:type] owner:newOwner];
}

@end

@implementation LocalPasteboard

+ (id)alloc
{
    return NSAllocateObject(self, 0, 0);
}

- (id)initWithName:(NSString *)name
{
    self = [super init];
    if (!self)
        return nil;

    _pasteboardName = adoptNS([name copy]);

    return self;
}

- (NSString *)name
{
    return _pasteboardName.get();
}

- (void)releaseGlobally
{
}

- (NSInteger)declareTypes:(NSArray *)newTypes owner:(id)newOwner
{
    _types.clear();
    _data.clear();

    return [self addTypes:newTypes owner:newOwner];
}

static bool isUTI(NSString *type)
{
    return UTTypeIsDynamic((__bridge CFStringRef)type) || UTTypeIsDeclared((__bridge CFStringRef)type);
}

static RetainPtr<NSString> toUTI(NSString *type)
{
    if (isUTI(type)) {
        // This is already an UTI.
        return adoptNS([type copy]);
    }

    return adoptNS((__bridge NSString *)UTTypeCreatePreferredIdentifierForTag(kUTTagClassNSPboardType, (__bridge CFStringRef)type, nullptr));
}

- (NSInteger)addTypes:(NSArray *)newTypes owner:(id)newOwner
{
    for (NSString *type in newTypes) {
        _types.add(toUTI(type));

        if (newOwner && [newOwner respondsToSelector:@selector(pasteboard:provideDataForType:)])
            [newOwner pasteboard:self provideDataForType:type];
    }

    return ++_changeCount;
}

- (NSInteger)changeCount
{
    return _changeCount;
}

- (NSArray *)types
{
    auto types = adoptNS([[NSMutableArray alloc] init]);

    for (const auto& type : _types) {
        [types addObject:type.get()];

        // Include the pasteboard type as well.
        if (auto pasteboardType = adoptNS((__bridge NSString *)UTTypeCopyPreferredTagWithClass((CFStringRef)type.get(), kUTTagClassNSPboardType)))
            [types addObject:pasteboardType.get()];
    }

    return types.autorelease();
}

- (NSString *)availableTypeFromArray:(NSArray *)types
{
    for (NSString *type in types) {
        if (_types.contains(type))
            return type;
    }

    return nil;
}

- (BOOL)setData:(NSData *)data forType:(NSString *)dataType
{
    auto uti = toUTI(dataType);

    if (!_types.contains(uti))
        return NO;

    _data.set(WTFMove(uti), data ? data : [NSData data]);
    ++_changeCount;
    return YES;
}

- (NSData *)dataForType:(NSString *)dataType
{
    return _data.get(toUTI(dataType)).get();
}

- (BOOL)setPropertyList:(id)propertyList forType:(NSString *)dataType
{
    CFDataRef data = NULL;
    if (propertyList)
        data = CFPropertyListCreateXMLData(NULL, propertyList);
    BOOL result = [self setData:(NSData *)data forType:dataType];
    if (data)
        CFRelease(data);
    return result;
}

- (BOOL)setString:(NSString *)string forType:(NSString *)dataType
{
    CFDataRef data = NULL;
    if (string) {
        if (!string.length)
            data = CFDataCreate(NULL, NULL, 0);
        else
            data = CFStringCreateExternalRepresentation(NULL, (CFStringRef)string, kCFStringEncodingUTF8, 0);
    }
    BOOL result = [self setData:(NSData *)data forType:dataType];
    if (data)
        CFRelease(data);
    return result;
}

- (BOOL)writeObjects:(NSArray<id <NSPasteboardWriting>> *)objects
{
    for (id <NSPasteboardWriting> object in objects) {
        for (NSString *type in [object writableTypesForPasteboard:self]) {
            ASSERT(UTTypeIsDeclared((__bridge CFStringRef)type) || UTTypeIsDynamic((__bridge CFStringRef)type));

            [self addTypes:@[ type ] owner:self];

            id propertyList = [object pasteboardPropertyListForType:type];
            if ([propertyList isKindOfClass:NSData.class])
                [self setData:propertyList forType:type];
            else
                ASSERT_NOT_REACHED();
        }
    }

    return YES;
}

@end

#endif // !PLATFORM(IOS)
