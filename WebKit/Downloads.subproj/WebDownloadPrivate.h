/*
    WebDownloadPrivate.h
    Copyright 2003, Apple, Inc. All rights reserved.

    Private header file.
*/

#import <WebKit/WebDownload.h>

@class WebDataSource;
@class WebResourceHandle;

@interface WebDownload (WebPrivate)
- _initWithLoadingHandle:(WebResourceHandle *)handle
                 request:(WebResourceRequest *)request
                response:(WebResourceRequest *)response
                delegate:(id <WebDownloadDelegate>)delegate;
// FIXME: Remove many of the following methods.
- initWithDataSource:(WebDataSource *)dSource;
- (WebError *)receivedData:(NSData *)data;
- (WebError *)finishedLoading;
- (void)decodeHeaderData:(NSData *)headerData
            dataForkData:(NSData **)dataForkData
        resourceForkData:(NSData **)resourceForkData;
- (BOOL)decodeData:(NSData *)data
      dataForkData:(NSData **)dataForkData
  resourceForkData:(NSData **)resourceForkData;
- (void)closeFork:(SInt16)forkRefNum deleteFile:(BOOL)deleteFile;
- (void)closeFileAndDelete:(BOOL)deleteFile;
- (void)closeFile;
- (void)cleanUpAfterFailure;
- (WebError *)createFileIfNecessary;
- (WebError *)writeDataForkData:(NSData *)dataForkData resourceForkData:(NSData *)resourceForkData;
- (NSData *)dataIfDoneBufferingData:(NSData *)data;
- (WebError *)decodeData:(NSData *)data;
- (BOOL)finishDecoding;
- (NSString *)path;
- (WebError *)errorWithCode:(int)code;
- (void)cancelWithError:(WebError *)error;
- (SInt16)dataForkReferenceNumber;
- (void)setDataForkReferenceNumber:(SInt16)forkRefNum;
- (SInt16)resourceForkReferenceNumber;
- (void)setResourceForkReferenceNumber:(SInt16)forkRefNum;
- (BOOL)writeForkData:(NSData *)data isDataFork:(BOOL)isDataFork;
- (BOOL)areWritesCancelled;
- (void)setWritesCancelled:(BOOL)cancelled;
@end
