/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "TestFilePromiseReceiver.h"

#if PLATFORM(MAC)

#import "DragAndDropSimulator.h"
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/darwin/DispatchExtras.h>

@implementation TestFilePromiseReceiver {
    RetainPtr<NSString> _promisedTypeIdentifier;
    RetainPtr<NSURL> _promisedFileURL;
    RetainPtr<NSURL> _destinationURL;
    WeakObjCPtr<id> _draggingSource;
}

- (instancetype)initWithTypeIdentifier:(NSString *)promisedTypeIdentifier fileURL:(NSURL *)promisedFileURL
{
    if (!(self = [super init]))
        return nil;

    _promisedFileURL = adoptNS([promisedFileURL copy]);
    _promisedTypeIdentifier = adoptNS([promisedTypeIdentifier copy]);
    return self;
}

- (NSArray<NSString *> *)fileTypes
{
    return @[ _promisedTypeIdentifier.get() ];
}

- (NSArray<NSString *> *)fileNames
{
    if (_destinationURL)
        return @[ [_destinationURL lastPathComponent] ];

    return @[ ];
}

- (void)dealloc
{
    if (_destinationURL)
        [[NSFileManager defaultManager] removeItemAtURL:_destinationURL.get() error:nil];

    [super dealloc];
}

- (id)draggingSource
{
    return _draggingSource.get().get();
}

- (void)setDraggingSource:(id)draggingSource
{
    _draggingSource = draggingSource;
}

static NSString *fileNameWithNumericSuffix(NSString *fileName, NSUInteger suffix)
{
    return [NSString stringWithFormat:@"%@ %zu.%@", fileName.stringByDeletingPathExtension, suffix, fileName.pathExtension];
}

static NSURL *writeToWebLoc(NSURL *webURL, NSURL *destinationDirectory, NSError **outError)
{
    auto components = adoptNS([webURL.absoluteString componentsSeparatedByCharactersInSet:NSCharacterSet.letterCharacterSet.invertedSet].mutableCopy);
    [components removeObject:@""];
    NSString *fileName = [[components componentsJoinedByString:@"-"] stringByAppendingPathExtension:@"webloc"];
    NSURL *destinationURL = [NSURL fileURLWithPath:fileName relativeToURL:destinationDirectory];
    return [@{ @"URL" : webURL.absoluteString } writeToURL:destinationURL error:outError] ? destinationURL : nil;
}

static NSURL *copyFile(NSURL *sourceURL, NSURL *destinationDirectory, NSError **outError)
{
    NSUInteger suffix = 0;
    NSString *fileName = sourceURL.lastPathComponent;
    NSURL *destinationURL = [NSURL fileURLWithPath:fileName relativeToURL:destinationDirectory];
    NSError *error;
    while (![NSFileManager.defaultManager copyItemAtURL:sourceURL toURL:destinationURL error:&error]) {
        if (error.domain != NSCocoaErrorDomain || error.code != NSFileWriteFileExistsError) {
            if (outError)
                *outError = error;
            return nil;
        }
        destinationURL = [NSURL fileURLWithPath:fileNameWithNumericSuffix(fileName, ++suffix) relativeToURL:destinationDirectory];
    }
    return destinationURL;
}

- (void)receivePromisedFilesAtDestination:(NSURL *)destinationDirectory options:(NSDictionary *)options operationQueue:(NSOperationQueue *)queue reader:(void (^)(NSURL *, NSError *))reader
{
    [queue addOperationWithBlock:^{
        NSError *error = nil;
        NSURL *destination = nil;
        if ([_promisedFileURL isFileURL])
            destination = copyFile(_promisedFileURL.get(), destinationDirectory, &error);
        else
            destination = writeToWebLoc(_promisedFileURL.get(), destinationDirectory, &error);
        if (destination) {
            dispatch_async(mainDispatchQueueSingleton(), ^{
                _destinationURL = destination;
            });
        }
        reader(destination, error);
    }];
}

@end

#endif // PLATFORM(MAC)
