/*
     WebDownload.h
     Copyright 2003, Apple, Inc. All rights reserved.

     Public header file.
*/

#import <Foundation/Foundation.h>

@class WebDownloadPrivate;
@class WebError;
@class NSURLRequest;
@class WebResponse;

@protocol WebDownloadDecisionListener;

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
    @method initWithRequest:
    @abstract Initializes a WebDownload object.
    @param request The request to download. Must not be nil.
*/
- initWithRequest:(NSURLRequest *)request;

/*!
    @method loadWithDelegate:
    @abstract Starts the download.
    @param delegate The delegate of the download. Must not be nil.
*/
- (void)loadWithDelegate:(id)delegate;

/*!
    @method cancel
    @abstract Cancels the download and deletes the downloaded file.
*/
- (void)cancel;

@end

/*!
    @protocol WebDownloadDelegate
    @discussion The WebDownloadDelegate delegate is primarily used to report the progress of the download.
*/
@interface NSObject (WebDownloadDelegate)

/*!
    @method download:didStartFromRequest:
    @abstract This method is called immediately after the download has started.
    @param download The download that just started downloading.
    @param request The request that the download started from.
 */
- (void)download:(WebDownload *)download didStartFromRequest:(NSURLRequest *)request;

/*!
    @method download:willSendRequest:
    @abstract This method is called whenever the download is about to load a request or if the download
    must load another request because the previous request was redirected.
    @discussion This method is called with a copy of the request that was passed with
    initWithRequest:. This method gives the delegate an opportunity to inspect the request
    that will be used to continue loading the request, and modify it if necessary.
    @param download The download that will send the request.
    @param request The request that will be used to continue loading.
    @result The request to be used; either the request parameter or a replacement. If nil is returned,
    the download is cancelled.
*/
- (NSURLRequest *)download:(WebDownload *)download willSendRequest:(NSURLRequest *)request;

/*!
    @method download:didReceiveResponse:
    @abstract This method is called when the download has received a response from the server.
    @param download The download that now has a WebResponse available for inspection.
    @param response The WebResponse object for the given download.
    @discussion In some rare cases, multiple responses may be received for a single download.
    This occurs with multipart/x-mixed-replace, or "server push". In this case, the client
    should assume that each new response resets progress so far for the resource back to 0,
    and should check the new response for the expected content length.
*/
- (void)download:(WebDownload *)download didReceiveResponse:(WebResponse *)response;

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
    @method downloadShouldDecodeEncodedFile:
    @abstract This method is called if the download detects that the downloading file is encoded.
    @discussion An encoded file is encoded in MacBinary, BinHex or gzip format.
    This method is not called if the download is not encoded.
    @param download The download that has detected that the downloading file is encoded.
    @result Return YES to decode the file, NO to not decode the file.
*/
- (BOOL)downloadShouldDecodeEncodedFile:(WebDownload *)download;

/*!
    @method download:didCreatedFileAtPath:
    @abstract This method is called after the download creates the download file.
    @discussion The filename of path may be different than the path set with setPath:
    so the download does not overwrite an existing file.
    @param download The download that created the downloading file.
    @param path The path of the downloading file.
*/
- (void)download:(WebDownload *)download didCreateFileAtPath:(NSString *)path;

/*!
    @method downloadDidFinishDownloading:
    @abstract This method is called when the download has finished downloading.
    @discussion This method is called after all the data has been received and written to disk.
    This method or download:didFailDownloadingWithError: will only be called once.
    @param download The download that has finished downloading.
*/
- (void)downloadDidFinishDownloading:(WebDownload *)download;

/*!
    @method download:didFailDownloadingWithError:
    @abstract This method is called when the download has failed. 
    @discussion This method is called when the download encounters a network or file I/O related error.
    This method or downloadDidFinishDownloading: will only be called once.
    @param download The download that ended in error.
    @param error The error caused the download to fail.
*/
- (void)download:(WebDownload *)download didFailDownloadingWithError:(WebError *)error;

@end

/*!
    @protocol WebDownloadDecisionListener
    @discussion An object that conforms to the WebDownloadDecisionListener protocol is passed
    with the download:decidePathWithListener:suggestedFilename: method of the WebDownloadDelegate protocol.
    There is no need to directly create an object that conforms to this protocol.
*/
@protocol WebDownloadDecisionListener <NSObject>
/*!
    @method setPath:
    @abstract This method should be called when the path of the downloaded file has been decided.
    @param path The path of the downloaded file.
    @discussion If necessary, to avoid overwriting files, "-n" (where "n" is a number) will be appended to the
    filename before the extension. Because of this, use the path passed with download:didCreateFileAtPath:
    when referring to the path of the downloaded file, not the path set with this method.
*/
-(void)setPath:(NSString *)path;

@end
