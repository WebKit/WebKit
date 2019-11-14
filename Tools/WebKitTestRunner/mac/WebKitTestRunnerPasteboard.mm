/*
 * Copyright (C) 2005-2018 Apple, Inc.  All rights reserved.
 * Copyright (C) 2007 Graham Dennis (graham.dennis@gmail.com)
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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
#import "WebKitTestRunnerPasteboard.h"

#import "NSPasteboardAdditions.h"
#import <objc/runtime.h>
#import <wtf/RetainPtr.h>

@interface LocalPasteboard : NSPasteboard
{
    RetainPtr<NSMutableArray> _typesArray;
    RetainPtr<NSMutableSet> _typesSet;
    RetainPtr<NSMutableArray<NSPasteboardItem *>> _writtenPasteboardItems;
    RetainPtr<NSMutableDictionary> _dataByType;
    NSInteger _changeCount;
    RetainPtr<NSString> _pasteboardName;
}

-(id)initWithName:(NSString *)name;
@end

static NSMutableDictionary *localPasteboards;

@implementation WebKitTestRunnerPasteboard

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

// This method crashes when called on LocalPasteboard.
// This happens during dragging, so overriding it may become unnecessary once we use mock dragging, like DumpRenderTree does.
- (void)_updateTypeCacheIfNeeded
{
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
    // Need to skip over [NSPasteboard alloc], which won't allocate a new object.
    return class_createInstance(self, 0);
}

- (id)initWithName:(NSString *)name
{
    self = [super init];
    if (!self)
        return nil;
    _typesArray = adoptNS([[NSMutableArray alloc] init]);
    _typesSet = adoptNS([[NSMutableSet alloc] init]);
    _dataByType = adoptNS([[NSMutableDictionary alloc] init]);
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

- (void)_clearContentsWithoutUpdatingChangeCount
{
    _writtenPasteboardItems = nil;
    [_typesArray removeAllObjects];
    [_typesSet removeAllObjects];
    [_dataByType removeAllObjects];
}

- (NSInteger)clearContents
{
    [self _clearContentsWithoutUpdatingChangeCount];
    return ++_changeCount;
}

- (NSInteger)declareTypes:(NSArray *)newTypes owner:(id)newOwner
{
    [self _clearContentsWithoutUpdatingChangeCount];
    [self _addTypesWithoutUpdatingChangeCount:newTypes owner:newOwner];
    return ++_changeCount;
}

- (NSInteger)addTypes:(NSArray<NSPasteboardType> *)newTypes owner:(id)newOwner
{
    [self _addTypesWithoutUpdatingChangeCount:newTypes owner:newOwner];
    // FIXME: Ideally, we would keep track of the current owner and only bump the change
    // count if the new owner is different.
    return ++_changeCount;
}

- (void)_addTypesWithoutUpdatingChangeCount:(NSArray *)newTypes owner:(id)newOwner
{
    unsigned count = [newTypes count];
    unsigned i;
    for (i = 0; i < count; ++i) {
        NSString *type = [newTypes objectAtIndex:i];
        NSString *setType = [_typesSet member:type];
        if (!setType) {
            setType = [type copy];
            [_typesArray addObject:setType];
            [_typesSet addObject:setType];
            [setType release];
        }
        if (newOwner && [newOwner respondsToSelector:@selector(pasteboard:provideDataForType:)])
            [newOwner pasteboard:self provideDataForType:setType];
    }
}

- (NSInteger)changeCount
{
    return _changeCount;
}

- (NSArray *)types
{
    return _typesArray.get();
}

- (NSString *)availableTypeFromArray:(NSArray *)types
{
    for (NSString *type in types) {
        if (NSString *setType = [_typesSet member:type])
            return setType;
    }
    return nil;
}

- (BOOL)setData:(NSData *)data forType:(NSString *)dataType
{
    if (![_typesSet containsObject:dataType])
        return NO;
    if (!data)
        data = [NSData data];
    [_dataByType setObject:data forKey:dataType];
    ++_changeCount;
    return YES;
}

- (NSData *)dataForType:(NSString *)dataType
{
    return [_dataByType objectForKey:dataType];
}

- (BOOL)setPropertyList:(id)propertyList forType:(NSString *)dataType
{
    NSData *data = nil;
    if (propertyList)
        data = [NSPropertyListSerialization dataWithPropertyList:propertyList format:NSPropertyListXMLFormat_v1_0 options:0 error:nullptr];
    return [self setData:data forType:dataType];
}

- (BOOL)setString:(NSString *)string forType:(NSString *)dataType
{
    return [self setData:[string dataUsingEncoding:NSUTF8StringEncoding] forType:dataType];
}

- (NSArray<NSPasteboardItem *> *)pasteboardItems
{
    if (_writtenPasteboardItems)
        return _writtenPasteboardItems.get();

    auto item = adoptNS([[NSPasteboardItem alloc] init]);
    for (NSString *type in _typesArray.get()) {
        NSPasteboardType modernPasteboardType = [NSPasteboard _modernPasteboardType:type];
        if (NSData *dataForType = [_dataByType objectForKey:type] ?: [_dataByType objectForKey:modernPasteboardType])
            [item setData:dataForType forType:modernPasteboardType];
    }
    return @[ item.get() ];
}

- (BOOL)writeObjects:(NSArray<id <NSPasteboardWriting>> *)objects
{
    _writtenPasteboardItems = adoptNS([[NSMutableArray<NSPasteboardItem *> alloc] initWithCapacity:objects.count]);
    for (id <NSPasteboardWriting> object in objects) {
        ASSERT([object isKindOfClass:NSPasteboardItem.class]);
        [_writtenPasteboardItems addObject:(NSPasteboardItem *)object];
        NSArray<NSPasteboardType> *writableTypes = [object writableTypesForPasteboard:self];
        for (NSString *type in writableTypes) {
            [self addTypes:@[type] owner:self];

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
