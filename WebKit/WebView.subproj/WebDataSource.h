/*	
        WebDataSource.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/

#import <Cocoa/Cocoa.h>

@class WebContentPolicy;
@class WebController;
@class WebDataSourcePrivate;
@class WebError;
@class WebFrame;
@class WebResourceHandle;
@class WebResourceRequest;

@protocol WebDocumentRepresentation;

/*!
    @class WebDataSource
    @discussion A WebDataSource represents the data associated with a web page.
    A datasource has a WebDocumentRepresentation which holds an appropriate
    representation of the data.  WebDataSources manage a hierarchy of WebFrames.
    WebDataSources are typically related to a view by there containg WebFrame.
*/
@interface WebDataSource : NSObject
{
@private
    WebDataSourcePrivate *_private;
}

/*!
    @method initWithURL:
    @discussion Returns nil if object cannot be initialized due to a malformed URL (RFC 1808).
    @param URL The URL to use in creating a datasource.
    @result Returns an initialized WebDataSource.
*/
- initWithURL:(NSURL *)URL;

/*!
    @method initWithRequest:
    @abstract The designated initializer for WebDataSource.
    @param request The request to use in creating a datasource.
    @result Returns an initialized WebDataSource.
*/
- initWithRequest:(WebResourceRequest *)request;

/*!
    @method data
    @discussion The data associated with a datasource will not be valid until
    a datasource has completed loaded.  
    @result Returns the raw data associated with this datasource.  Returns nil
    if the datasource hasn't loaded.
*/
- (NSData *)data;

/*!
    @method representation
    @discussion A representation holds a type specific representation
    of the datasource's data.  The representation class is determined by mapping
    a MIME type to a class.  The representation is created once the MIME type
    of the datasource content has been determined.
    @result Returns the representation associated with this datasource.
    Returns nil if the datasource hasn't created it's representation.
*/
- (id <WebDocumentRepresentation>)representation;

/*!
    @method parent
    @result Returns nil if this data source represents the main document.  Otherwise
    returns the parent data source.
*/
- (WebDataSource *)parent;

/*!
    @method webFrame
    @result Return the frame that represents this data source.
*/
- (WebFrame *)webFrame;

/*!
    @method children
    @discussion The frames in the array are
    associated with a frame set or iframe.
    @result Returns an array of WebFrame.
*/
- (NSArray *)children;

/*!
    @method frameNamed:
    @discussion Find the frame named frameName in this datasource's
    children.  Does not recurse.
    @param frameName The name of the frame to find.
    @result Returns the frame named frameName, or nil.
*/
- (WebFrame *)frameNamed:(NSString *)frameName;

/*!
    @method frameNames
    @discussion The NSStrings corresponds to
    frame names.  If this data source is the main document and has no
    frames then frameNames will return nil.
    @result Returns an array of NSStrings or nil.
*/
- (NSArray *)frameNames;

/*!
    @method findDataSourceForFrameNamed:
    @param name The name of the frame to find. Does not recurse.
    @result Returns the child data source associated with
    the frame named 'name', or nil. 
*/
- (WebDataSource *)findDataSourceForFrameNamed:(NSString *)name;

/*!
    @method frameExists:
    @param name The name of the frame to check for existence.  Does not recurse.
    @result Returns YES if a frame named name exists.
*/
- (BOOL)frameExists: (NSString *)name;

/*!
    @method controller
    @result Returns the controller associated with this datasource.
*/
- (WebController *)controller;
    
/*!
    @method request
    @result Returns the request that was used to create this datasource.
*/
-(WebResourceRequest *)request;

/*!
    @method URL
    @discussion The value of URL will change if a redirect occurs.
    To monitor change in the URL, override the <WebLocationChangeHandler> 
    serverRedirectTo:forDataSource: method.
    @result Returns the current URL associated with the datasource.
*/
- (NSURL *)URL;

/*!
    @method startLoading
    @discussion Start actually getting and parsing data. If the data source
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
    @result Returns YES if the representation of the datasource is a WebHTMLRepresentation.
*/
- (BOOL)isDocumentHTML;

/*!
    @method encoding
    // FIXME rename to textEncodingName, move to WebHTMLRepresentation
*/
- (NSString *)encoding;

/*!
    @method pageTitle
    @result Returns nil or the page title.
    // FIXME move to WebHTMLRepresentation
*/
- (NSString *)pageTitle;

/*!
    @method frameName
    @result frameName The name of frame that contains this datasource.
*/
- (NSString *)frameName;

/*!
    @method contentPolicy
    @result The content policy used by this datasource.
*/
- (WebContentPolicy *)contentPolicy;

/*!
    @method contentType
    @result returns the MIME type for the data source.
*/
- (NSString *)contentType;

/*!
    @method fileType
    @result The extension based on the MIME type 
*/
- (NSString *)fileType;

/*!
    @method errors
    @result Returns a dictionary of WebErrors from all the resources loaded for this page.
*/
- (NSDictionary *)errors;

/*!
    @method mainDocumentError
    @result Returns a WebError associated with the load of the main document, or nil if no error occurred.
*/
- (WebError *)mainDocumentError;

/*!
    @method registerRepresentationClass:forMIMEType:
    @discussion A class that implements WebDocumentRepresentation may be registered 
    with this method.
    A document class may register for a primary MIME type by excluding
    a subtype, i.e. "video/" will match the document class with
    all video types.  More specific matching takes precedence
    over general matching.
    @param repClass
    @param MIMEType
*/
+ (void) registerRepresentationClass:(Class)repClass forMIMEType:(NSString *)MIMEType;

@end
