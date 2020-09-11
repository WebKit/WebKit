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

#import "config.h"
#import "QuickLookThumbnailLoader.h"

#if HAVE(QUICKLOOK_THUMBNAILING)

#import <wtf/FileSystem.h>

#import "QuickLookThumbnailingSoftLink.h"

@implementation WKQLThumbnailQueueManager 

- (instancetype)init
{
    self = [super init];
    if (self)
        _queue = [[NSOperationQueue alloc] init];
    return self;
}

- (void)dealloc
{
    [_queue release];
    [super dealloc];
}

+ (WKQLThumbnailQueueManager *)sharedInstance
{
    static WKQLThumbnailQueueManager *sharedInstance = [[WKQLThumbnailQueueManager alloc] init];
    return sharedInstance;
}

@end

@interface WKQLThumbnailLoadOperation ()
@property (atomic, readwrite, getter=isExecuting) BOOL executing;
@property (atomic, readwrite, getter=isFinished) BOOL finished;
@end

@implementation WKQLThumbnailLoadOperation {
    RetainPtr<NSURL> _filePath;
    RetainPtr<NSString> _identifier;
    RetainPtr<NSFileWrapper> _fileWrapper;
    RetainPtr<CocoaImage> _thumbnail;
    BOOL _shouldWrite;
}

- (instancetype)initWithAttachment:(NSFileWrapper *)fileWrapper identifier:(NSString *)identifier
{
    if (self = [super init]) {
        _fileWrapper = fileWrapper;
        _identifier = adoptNS([identifier copy]);
        _shouldWrite = YES;
    }
    return self;
}

- (instancetype)initWithURL:(NSString *)fileURL identifier:(NSString *)identifier
{
    if (self = [super init]) {
        _identifier = adoptNS([identifier copy]);
        _filePath = [NSURL fileURLWithPath:fileURL];
    }
    return self;
}

- (void)start
{
    self.executing = YES;

    if (_shouldWrite) {
        NSString *temporaryDirectory = FileSystem::createTemporaryDirectory(@"QLTempFileData");

        NSString *filePath = [temporaryDirectory stringByAppendingPathComponent:[_fileWrapper preferredFilename]];
        NSFileWrapperWritingOptions options = 0;
        NSError *error = nil;
        
        auto fileURLPath = [NSURL fileURLWithPath:filePath];

        [_fileWrapper writeToURL:fileURLPath options:options originalContentsURL:nil error:&error];
        _filePath = WTFMove(fileURLPath);
        if (error)
            return;
    }

    auto request = adoptNS([WebKit::allocQLThumbnailGenerationRequestInstance() initWithFileAtURL:_filePath.get() size:CGSizeMake(400, 400) scale:1 representationTypes:QLThumbnailGenerationRequestRepresentationTypeThumbnail]);
    [request setIconMode:YES];
    
    [[WebKit::getQLThumbnailGeneratorClass() sharedGenerator] generateBestRepresentationForRequest:request.get() completionHandler:^(QLThumbnailRepresentation *thumbnail, NSError *error) {
        if (error)
            return;
        if (_thumbnail)
            return;
#if USE(APPKIT)
        _thumbnail = thumbnail.NSImage;
#else
        _thumbnail = thumbnail.UIImage;
#endif
        if (_shouldWrite)
            [[NSFileManager defaultManager] removeItemAtURL:_filePath.get() error:nullptr];

        self.executing = NO;
        self.finished = YES;
    }];
}

- (CocoaImage *)thumbnail
{
    return _thumbnail.get();
}

- (NSString *)identifier
{
    return _identifier.get();
}

- (BOOL)isAsynchronous
{
    return YES;
}

@synthesize executing=_executing;

- (BOOL)isExecuting
{
    @synchronized(self) {
        return _executing;
    }
}

- (void)setExecuting:(BOOL)executing
{
    @synchronized(self) {
        if (executing != _executing) {
            [self willChangeValueForKey:@"isExecuting"];
            _executing = executing;
            [self didChangeValueForKey:@"isExecuting"];
        }
    }
}

@synthesize finished=_finished;

- (BOOL)isFinished
{
    @synchronized(self) {
        return _finished;
    }
}

- (void)setFinished:(BOOL)finished
{
    @synchronized(self) {
        if (finished != _finished) {
            [self willChangeValueForKey:@"isFinished"];
            _finished = finished;
            [self didChangeValueForKey:@"isFinished"];
        }
    }
}

@end

#endif // HAVE(QUICKLOOK_THUMBNAILING)
