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

/*! @abstract A @link WKWebView @/link's list of previously-visited webpages that can be reached by
 going back or forward.
 */
WK_API_CLASS
@interface WKBackForwardList : NSObject

/*! @abstract The current item.
 */
@property (nonatomic, readonly) WKBackForwardListItem *currentItem;

/*! @abstract The item right before the current item, or nil if there isn't one.
 */
@property (nonatomic, readonly) WKBackForwardListItem *backItem;

/*! @abstract The item right after the current item, or nil if there isn't one.
 */
@property (nonatomic, readonly) WKBackForwardListItem *forwardItem;

/*! @abstract Returns an entry the given distance from the current entry.
 @param index Index of the desired list item relative to the current item; 0 is current item, -1 is back item, 1 is forward item, etc.
 @result The entry the given distance from the current entry. If index exceeds the limits of the list, nil is returned.
 */
- (WKBackForwardListItem *)itemAtIndex:(NSInteger)index;

/*! @abstract Returns the portion of the list before the current entry.
 @discussion The entries are in the order that they were originally visited.
 */
@property (nonatomic, readonly) NSArray *backList;

/*! @abstract Returns the portion of the list after the current entry.
 @discussion The entries are in the order that they were originally visited.
 */
@property (nonatomic, readonly) NSArray *forwardList;

@end

#endif // WK_API_ENABLED
