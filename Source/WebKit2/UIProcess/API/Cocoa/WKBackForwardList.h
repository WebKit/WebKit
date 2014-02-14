/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#import <WebKit2/WKFoundation.h>

#if WK_API_ENABLED

#import <WebKit2/WKBackForwardListItem.h>

/*! Posted when a back-forward list changes. The notification object is the WKBackForwardList
        that changed. The <code>userInfo</code> dictionary may contain the
        @link WKBackForwardListAddedItemKey @/link and @link WKBackForwardListRemovedItemsKey @/link
        keys.
*/
WK_EXTERN NSString * const WKBackForwardListDidChangeNotification;

/*! A key in the <code>userInfo</code> dictionary of a
        @link WKBackForwardListDidChangeNotification @/link, whose value is the
        @link WKBackForwardListItem @/link that was appended to the list.
*/
WK_EXTERN NSString * const WKBackForwardListAddedItemKey;


/*! A key in the <code>userInfo</code> dictionary of a
        @link WKBackForwardListDidChangeNotification @/link, whose value is an NSArray of
        @link WKBackForwardListItem@/link instances that were removed from the list.
*/
WK_EXTERN NSString * const WKBackForwardListRemovedItemsKey;

WK_API_CLASS
@interface WKBackForwardList : NSObject

@property (readonly) WKBackForwardListItem *currentItem;
@property (readonly) WKBackForwardListItem *backItem;
@property (readonly) WKBackForwardListItem *forwardItem;

- (WKBackForwardListItem *)itemAtIndex:(NSInteger)index;

@property (readonly) NSUInteger backListCount;
@property (readonly) NSUInteger forwardListCount;

- (NSArray *)backListWithLimit:(NSUInteger)limit;
- (NSArray *)forwardListWithLimit:(NSUInteger)limit;

@end

#endif // WK_API_ENABLED
