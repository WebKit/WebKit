/*	
        WebDataSource.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/

#import <Cocoa/Cocoa.h>

#import <WebKit/WebController.h>

@class WebFrame;
@class WebResourceHandle;
@class WebController;
@class WebDataSourcePrivate;
@class WebResourceRequest;

@protocol WebDocumentRepresentation;

/*!
    @class WebDataSource
    @discussion A WebDataSource represents the data associated with a web page.
*/
@interface WebDataSource : NSObject
{
@private
    WebDataSourcePrivate *_private;
}

/*!
    @method initWithURL:
    @discussion Returns nil if object cannot be initialized due to a malformed URL (RFC 1808).
    @param URL
*/
-(id)initWithURL:(NSURL *)URL;

/*!
    @method initWithRequest:
    @param request
*/
-(id)initWithRequest:(WebResourceRequest *)request;

/*!
    @method data
*/
- (NSData *)data;

/*!
    @method representation
*/
- (id <WebDocumentRepresentation>)representation;

/*!
    @method isMainDocument
    @discussion Returns YES if this is the main document.  The main document is the 'top'
    document, typically either a frameset or a normal HTML document.
*/
- (BOOL)isMainDocument;

/*!
    @method parent
    @discussion Returns nil if this data source represents the main document.  Otherwise
    returns the parent data source.
*/
- (WebDataSource *)parent;

/*!
    @method webFrame
    @discussion Return the frame that represents this data source. Same as above.
*/
- (WebFrame *)webFrame;

/*!
    @method addFrame:
    @discussion Add a child frame.  This should only be called by the data source's controller
    as a result of a createFrame:inParent:.
    // [Should this be private?]
*/
- (void)addFrame: (WebFrame *)frame;

/*!
    @method children
    @discussion Returns an array of WebFrame.  The frames in the array are
    associated with a frame set or iframe.
*/
- (NSArray *)children;

/*!
    @method frameNamed:
    @param frameName
*/
- (WebFrame *)frameNamed:(NSString *)frameName;

/*!
    @method frameNames
    @discussion Returns an array of NSStrings or nil.  The NSStrings corresponds to
    frame names.  If this data source is the main document and has no
    frames then frameNames will return nil.
*/
- (NSArray *)frameNames;

/*!
    @method findDataSourceForFrameNamed:
    @discussion findDataSourceForFrameNamed: returns the child data source associated with
    the frame named 'name', or nil. 
    @param name
*/
- (WebDataSource *)findDataSourceForFrameNamed:(NSString *)name;

/*!
    @method frameExists:
    @param name
*/
- (BOOL)frameExists: (NSString *)name;

/*!
    @method openURL:isFrameNamed:
    @param URL
    @param frameName
*/
- (void)openURL:(NSURL *)URL inFrameNamed:(NSString *)frameName;

/*!
    @method controller
*/
- (WebController *)controller;
    
/*!
    @method request
*/
-(WebResourceRequest *)request;

/*!
    @method URL
    @discussion May return nil if not initialized with a URL.
    The value of URL will change if a redirect occurs.
    To monitor change in the URL, override the <WebLocationChangeHandler> 
    serverRedirectTo:forDataSource: method.
*/
- (NSURL *)URL;

/*!
    @method originalURL
    @discussion The original passed in at initialization time.
    Starts out same as URL, but doesn't change if a redirect occurs.
*/
- (NSURL *)originalURL;

/*!
    @method startLoading
    @discussion Start actually getting (if initialized with a URL) and parsing data. If the data source
    is still performing a previous load it will be stopped.
*/
- (void)startLoading;

/*!
    @method stopLoading
    @discussion Cancels any pending loads.  A data source is conceptually only ever loading
    one document at a time, although one document may have many related
    resources.  stopLoading will stop all loads related to the data source.
*/
- (void)stopLoading;

/*!
    @method isLoading
    @discussion Returns YES if there are any pending loads.
*/
- (BOOL)isLoading;

/*!
    @method isDocumentHTML
*/
- (BOOL)isDocumentHTML;

/*!
    @method documentSource
    @abstract Get the actual source of the document.
*/
- (NSString *)documentSource;

/*!
    @method base	
    @abstract URL reference point, these should probably not be public for 1.0.
*/
- (NSURL *)base;

/*!
    @method baseTarget
*/
- (NSString *)baseTarget;

/*!
    @method encoding
*/
- (NSString *)encoding;

/*!
    @method setUserStyleSheetFromURL:
    @param URL
*/
- (void)setUserStyleSheetFromURL: (NSURL *)URL;

/*!
    @method setUserStyleSheetFromString:
    @param sheet
*/
- (void)setUserStyleSheetFromString: (NSString *)sheet;

/*!
    @method icon
    @discussion a.k.a shortcut icons, http://msdn.microsoft.com/workshop/Author/dhtml/howto/ShortcutIcon.asp.
    This method may be moved to a category to prevent unnecessary linkage to the AppKit.  Note, however
    that WebCore also has dependencies on the appkit.
*/
- (NSImage *)icon;

/*!
    @method isPageSecure
    @discussion Is page secure, e.g. https, ftps
*/
- (BOOL)isPageSecure;

/*!
    @method pageTitle
    @discussion Returns nil or the page title.
*/
- (NSString *)pageTitle;

/*!
    @method frameName
*/
- (NSString *)frameName;

/*!
    @method contentPolicy
*/
- (WebContentPolicy *)contentPolicy;

/*!
    @method contentType
    @discussion returns the MIME type for the data source.
*/
- (NSString *)contentType;

/*!
    @method fileType
    @discussion extension based on the MIME type 
*/
- (NSString *)fileType;

/*!
    @method errors
*/
- (NSDictionary *)errors;

/*!
    @method mainDocumentError
*/
- (WebError *)mainDocumentError;

/*!
    @method registerRepresentationClass:forMIMEType:
    @param repClass
    @param MIMEType
*/
+ (void) registerRepresentationClass:(Class)repClass forMIMEType:(NSString *)MIMEType;

@end
