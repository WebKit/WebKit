/*
    WebDownloadPrivate.h
    Copyright 2003, Apple, Inc. All rights reserved.

    Private header file.
*/

#import <WebKit/WebDownload.h>

@interface WebDownload (WebPrivate)
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
