/*	IFAttributedURL.h
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#ifdef NEW_WEBKIT_API

//=============================================================================
//
// IFAttributedURL.h
//
// IFAttributedURL is the class that is used to store a "reference" to a URL
// in the various history and bookmark mechanisms. It provides some attributes
// that clients can use to "decorate" a URL with data that a user might find 
// useful in organizing their internet and browsing experience.
// 
@interface IFAttributedURL : NSObject
{
    // The URL that serves as the "anchor" for the accompanying attributes.
    NSURL *url;

    // The human-readable title of the URL. Taken from the <TITLE> tag
    // in the case of an HTML document.
    NSString *title;

    // An icon to associate with the URL. Can be determined by mime type,
    // file extensions, or the various and sundry "favicon.ico" strategies.
    NSImage *image;

    // A place for users to add their own comments.
    NSString *comment;

    // The date of object creation.
    NSDate *creationDate;

    // The date that any of the settable fields was last changed.
    NSDate *modificationDate;

    // The date that the URL was last visited by the user.
    NSDate *lastVisitedDate;
    
    // The total number of times the URL has been visited.
    int visits;
}

//
// Initializes a IFAttributedURL object, setting the URL to the given
// object, the creation and modification dates to "now" and all other 
// fields to empty values (nil or zero).
//
-(id)initWithURL:(NSURL *)url;

//
// Initializes a IFAttributedURL object, setting the URL and title to 
// the given objects, the creation and modification dates to "now" and 
// all other fields to empty values (nil or zero).
//
-(id)initWithURL:(NSURL *)url title:(NSString *)title;

//
// Returns the URL that serves as the "anchor" for the accompanying 
// attributes.
//
-(NSURL *)url;

//
// Returns the human-readable title of the URL, or nil if no title 
// has been set
//
-(NSString *)title;

//
// Returns the icon associated with the URL, or nil if no icon image
// has been set. Note that the image can be determined by mime type,
// file extensions, or the various and sundry "favicon.ico" strategies.
//
-(NSImage *)image;

//
// Returns the comment string set by the user, if any, or nil if no
// comment has been set.
//
-(NSString *)comment;

//
// Returns the date of object creation. This value is immutable.
//
-(NSDate *)creationDate;

//
// Returns the date that any of the settable fields was last changed.
//
-(NSDate *)modificationDate;

//
// Returns the date that the URL was last visited by the user, or nil
// if the URL has never been visited.
//
-(NSDate *)lastVisitedDate;

//
// Returns the total number of times the URL has been visited.
//
-(int)visits;

//
// Sets the URL of the receiver to the given object.
//
-(void)setURL:(NSURL *)aURL;

//
// Sets the title of the receiver to the given object.
//
-(void)setTitle:(NSString *)aTitle;

//
// Sets the image of the receiver to the given object.
//
-(void)setImage:(NSImage *)anImage;

//
// Sets the comment of the receiver to the given object.
//
-(void)setComment:(NSString *)aComment;

//
// Sets the modification date of the receiver to the given object.
//
-(void)setModificationDate:(NSDate *)aDate;

//
// Sets the last-visited date of the receiver to the given object.
//
-(void)setLastVisitedDate:(NSDate *)aDate;

//
// Increments the visited field of the receiver by 1 (one).
//
-(void)incrementVisits;

//
// Returns a hash code for the receiver.
//
-(unsigned)hash;

//
// Returns YES if the receiver is equal to the given object, no otherwise. 
// Two IFAttributedURL objects are equal if and only if they have the 
// equal URLs and creation dates.
//
-(BOOL)isEqual:(id)anObject;

@end

//=============================================================================

#endif // NEW_WEBKIT_API
