/*	
    WebHistoryItem.h
    Copyright (C) 2003 Apple Computer, Inc. All rights reserved.    

    Public header file.
 */

#import <Cocoa/Cocoa.h>

@class WebHistoryItemPrivate;
@class NSURL;

/*!
    @class WebHistoryItem
    @discussion  WebHistoryItems are created by WebKit to represent pages visited.
    The WebBackForwardList and WebHistory classes both use WebHistoryItems to represent
    pages visited.  With the exception of the displayTitle, the properties of 
    WebHistoryItems are set by WebKit.
    WebHistoryItems cannot be created directly.
*/
@interface WebHistoryItem : NSObject <NSCopying>
{
@private
    WebHistoryItemPrivate *_private;
}

/*!
    @method URLString
    @abstract The string representation of the URL represented by this item.
    @discussion The URLString may be different than the originalURLString if the page
    redirected to a new location.
*/
- (NSString *)URLString;

/*!
    @method originalURLString
    @abstract The string representation of the originial URL of this item.
*/
- (NSString *)originalURLString;

/*!
    @method title
    @abstract The title of the page represented by this item.
    @discussion This title cannot be changed by the client.
*/
- (NSString *)title;

/*!
    @method setAlternateTitle:
    @param alternateTitle The new display title for this item.
    @abstract A title that may be used by the client to display this item.
*/
- (void)setAlternateTitle:(NSString *)alternateTitle;

/*
    @method title
    @abstract A title that may be used by the client to display this item.
*/
- (NSString *)alternateTitle;

/*!
    @method icon
    @abstract The favorite icon of the page represented by this item.
*/
- (NSImage *)icon;

/*!
    @method lastVisitedDate
    @abstract The last time the page represented by this item was visited.
*/
- (NSCalendarDate *)lastVisitedDate;

/*!
    @method anchor
    @abstract The name of the fragment anchor, if any, represented by the URL for this item.
*/
- (NSString *)anchor;

@end
