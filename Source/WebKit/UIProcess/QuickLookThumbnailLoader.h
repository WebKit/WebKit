/*
* Copyright (C) 2020 Apple Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
* THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
* THE POSSIBILITY OF SUCH DAMAGE.
*/

#if HAVE(QUICKLOOK_THUMBNAILING)

#if PLATFORM(IOS_FAMILY)
@class UIImage;
#endif

@interface WKQLThumbnailQueueManager : NSObject

@property (nonatomic, readwrite, retain) NSOperationQueue* qlThumbnailGenerationQueue;
- (id)init;
+ (WKQLThumbnailQueueManager *)sharedInstance;
@end

@interface WKQLThumbnailLoadOperation : NSOperation

@property (readonly, getter=isAsynchronous) BOOL asynchronous;
@property (readonly, getter=isExecuting) BOOL executing;
@property (readonly, getter=isFinished) BOOL finished;

@property (nonatomic, copy) NSString *contentType;
@property (nonatomic) BOOL shouldWrite;

- (id)initWithAttachment:(NSFileWrapper *)fileWrapper identifier:(NSString *)identifier;
- (id)initWithURL:(NSString *)fileURL identifier:(NSString *)identifier;
- (NSString *)identifier;
#if PLATFORM(IOS_FAMILY)
-(UIImage *)thumbnail;
#endif
#if PLATFORM(MAC)
-(NSImage *)thumbnail;
#endif

@end

#endif
