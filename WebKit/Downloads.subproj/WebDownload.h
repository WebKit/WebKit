/*
     WebDownload.h
     Copyright 2003, Apple, Inc. All rights reserved.

     Public header file.
*/

#import <Foundation/Foundation.h>

@class WebDownload;
@class WebDownloadPrivate;
@class WebError;
@class WebResourceRequest;
@class WebResourceResponse;

/*!
    @protocol WebDownloadDecisionListener
    @discussion An object that conforms to the WebDownloadDecisionListener protocol is passed
    with the download:decidePathWithListener: method of the WebDownloadDelegate protocol. There is
    no need to directly create an object that conforms to this protocol.
*/
@protocol WebDownloadDecisionListener <NSObject>
/*!
    @method setPath:
    @abstract This method should be called when the path of the downloaded file has been decided.
    @param path The path of the downloaded file.
*/
-(void)setPath:(NSString *)path;
@end

/*!
    @protocol WebDownloadDelegate
    @discussion The delegate of a WebDownload must conform to the WebDownloadDelegate protocol.
    The delegate is primarily used to report the progress of the download. Note: The word "download" is used to
    refer to the process of loading data off a network, decoding the data if necessary and saving the data to a file.
    The delegate recieves these calls in this order:

    - download:willSendRequest: (called once or more)<BR>
    - download:didReceiveResponse:<BR>
    - download:decidePathWithListener:suggestedFilename: (possibly not called)<BR>
    - download:didReceiveDataOfLength: (called once or more)<BR>
    - downloadDidFinishLoading: or - download:didFailLoadingWithError:
*/
@protocol WebDownloadDelegate <NSObject>
/*!
    @method download:willSendRequest:
    @abstract This method is called whenever the download is about to load a request or if the download
    must load another request because the previous request was redirected.
    @discussion This method is always initially called with a copy of the request that was passed with
    initWithRequest:delegate:. This method gives the delegate an opportunity to inspect the request
    that will be used to continue loading the request, and modify it if necessary.
    @param download The download that will send the request.
    @param request The request that will be used to continue loading.
    @result The request to be used; either the request parameter or a replacement.
*/
- (WebResourceRequest *)download:(WebDownload *)download willSendRequest:(WebResourceRequest *)request;

/*!
    @method download:didReceiveResponse:
    @abstract This method is called when the download has received enough information to contruct a WebResourceResponse.
    @param download The download that now has a WebResourceResponse available for inspection.
    @param response The WebResourceResponse object for the given download.
*/
- (void)download:(WebDownload *)download didReceiveResponse:(WebResourceResponse *)response;

/*!
    @method download:decidePathWithListener:suggestedFilename:
    @abstract This method is called when enough information has been loaded to decide a path for the download.
    @discussion Once the delegate has decided a path, it should call setPath: on the passed listener object.
    The delegate can either respond immediately, or retain the listener and respond later. This method is not
    called if the download has a predetermined path (i.e. image downloads via drag & drop).
    @param download The download that requests the download path.
    @param filename The suggested filename for deciding the path of the downloaded file. The filename is either
    derived from the last path component of the URL and the MIME type or if the download was encoded,
    it is the filename specified in the encoding.
*/
- (void)download:(WebDownload *)download decidePathWithListener:(id <WebDownloadDecisionListener>)listener suggestedFilename:(NSString *)filename;

/*!
    @method download:didReceiveDataOfLength:
    @abstract This method is called when the download has loaded data.
    @discussion This method will be called 1 or more times.
    @param download The download that has received data.
    @param length The length of the received data.
*/
- (void)download:(WebDownload *)download didReceiveDataOfLength:(unsigned)length;

/*!
    @method downloadDidFinishLoading:
    @abstract This method is called when the download has finished downloading.
    @discussion This method is called after all the data has been received and written to disk.
    This method or download:didFailLoadingWithError: will only be called once.
    @param download The download that has finished downloading.
*/
- (void)downloadDidFinishLoading:(WebDownload *)download;

/*!
    @method download:didFailLoadingWithError:
    @abstract This method is called when the download has failed. 
    @discussion This method is called when the download encounters a network or file I/O related error.
    This method or downloadDidFinishLoading: will only be called once.
    @param download The download that ended in error.
    @param error The error caused the download to fail.
*/
- (void)download:(WebDownload *)download didFailLoadingWithError:(WebError *)error;
@end

/*!
    @class WebDownload
    @discussion A WebDownload loads a request and saves the resource to a file. The progress of the download
    is reported via the WebDownloadDelegate protocol. Note: The word "download" is used to refer to the process
    of loading data off a network, decoding the data if necessary and saving the data to a file.
*/
@interface WebDownload : NSObject
{
@private
    WebDownloadPrivate *_private;
}

/*!
    @method initWithRequest:delegate:
    @abstract Initializes a WebDownload object.
    @discussion This method also begins the download.
    @param request The request to download. Must not be nil.
    @param delegate The delegate of the download. Must not be nil.
*/
- initWithRequest:(WebResourceRequest *)request delegate:(id <WebDownloadDelegate>)delegate;

/*!
    @method cancel
    @abstract Cancels the download and deletes the downloaded file.
    @discussion Has no effect after the download has completed.
*/
- (void)cancel;

@end
