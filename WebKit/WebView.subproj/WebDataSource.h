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
@class WebResource;
@class WebRequest;
@class WebResponse;

@protocol WebDocumentRepresentation;

/*!
    @class WebDataSource
    @discussion A WebDataSource represents the data associated with a web page.
    A datasource has a WebDocumentRepresentation which holds an appropriate
    representation of the data.  WebDataSources manage a hierarchy of WebFrames.
    WebDataSources are typically related to a view by their containing WebFrame.
*/
@interface WebDataSource : NSObject
{
@private
    WebDataSourcePrivate *_private;
}

/*!
    @method initWithRequest:
    @abstract The designated initializer for WebDataSource.
    @param request The request to use in creating a datasource.
    @result Returns an initialized WebDataSource.
*/
- (id)initWithRequest:(WebRequest *)request;

/*!
    @method data
    @discussion The data associated with a datasource will not be valid until
    a datasource has completely loaded.  
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
    @method initialRequest
    @result Returns a reference to the original request that created the
    datasource.  This request will be unmodified by WebKit. 
*/
- (WebRequest *)initialRequest;

/*!
    @method request
    @result Returns the request that was used to create this datasource.
*/
-(WebRequest *)request;

/*!
    @method response
    @result returns the WebResourceResponse for the data source.
*/
- (WebResponse *)response;

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

@end
