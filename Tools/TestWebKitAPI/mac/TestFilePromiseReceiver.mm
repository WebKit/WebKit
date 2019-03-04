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

@implementation TestFilePromiseReceiver {
    RetainPtr<NSArray<NSString *>> _promisedTypeIdentifiers;
    RetainPtr<NSMutableArray<NSURL *>> _destinationURLs;
    WeakObjCPtr<id> _draggingSource;
    WeakObjCPtr<DragAndDropSimulator> _dragAndDropSimulator;
}

- (instancetype)initWithPromisedTypeIdentifiers:(NSArray<NSString *> *)promisedTypeIdentifiers dragAndDropSimulator:(DragAndDropSimulator *)simulator
{
    if (!(self = [super init]))
        return nil;

    _dragAndDropSimulator = simulator;
    _promisedTypeIdentifiers = adoptNS([promisedTypeIdentifiers copy]);
    _destinationURLs = adoptNS([NSMutableArray new]);
    return self;
}

- (NSArray<NSString *> *)fileTypes
{
    return _promisedTypeIdentifiers.get();
}

- (NSArray<NSString *> *)fileNames
{
    NSMutableArray *fileNames = [NSMutableArray arrayWithCapacity:[_destinationURLs count]];
    for (NSURL *url in _destinationURLs.get())
        [fileNames addObject:url.lastPathComponent];
    return fileNames;
}

- (void)dealloc
{
    for (NSURL *destinationURL in _destinationURLs.get())
        [[NSFileManager defaultManager] removeItemAtURL:destinationURL error:nil];

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
    for (NSURL *sourceURL in [_dragAndDropSimulator externalPromisedFiles]) {
        [queue addOperationWithBlock:^{
            NSError *error = nil;
            NSURL *destination = copyFile(sourceURL, destinationDirectory, &error);
            if (destination) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    [_destinationURLs addObject:destination];
                });
            }
            reader(destination, error);
        }];
    }
}

@end

#endif // PLATFORM(MAC)
