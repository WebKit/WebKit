/*	
        WebDataSource.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/

#import <Cocoa/Cocoa.h>

@class WebController;
@class WebDataSourcePrivate;
@class WebError;
@class WebFrame;
@class WebResourceHandle;
@class WebResourceRequest;
@class WebResourceResponse;

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
    @method webFrame
    @result Return the frame that represents this data source.
*/
- (WebFrame *)webFrame;

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
    @method response
    @result returns the WebResourceResponse for the data source.
*/
- (WebResourceResponse *)response;

/*!
    @method URL
    @discussion The value of URL will change if a redirect occurs.
    To monitor change in the URL, override the <WebLocationChangeHandler> 
    serverRedirectedForDataSource: method.
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
    @method fileExtension
    @result The extension based on the MIME type 
*/
- (NSString *)fileExtension;

/*!
    @method mainDocumentError
    @result Returns a WebError associated with the load of the main document, or nil if no error occurred.
*/
- (WebError *)mainDocumentError;

/*!
    @method stringWithData:
    @result A string decoded using the determined encoding.
    @discussion The overidden encoding is used if one is present. If no overidden encoding is specified,
    the server specified encoding is used. If no server specified encoding is specified,
    kCFStringEncodingISOLatin1 is used.
*/
- (NSString *)stringWithData:(NSData *)data;


- (BOOL)isDownloading;
- (NSString *)downloadPath;


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
