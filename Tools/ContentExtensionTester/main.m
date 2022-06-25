/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#import <Foundation/Foundation.h>
#import <WebKit/_WKUserContentExtensionStorePrivate.h>

static void run(bool* done)
{
    while (!*done)
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]];
}

int main(int argc, const char* argv[])
{
    @autoreleasepool {
        if (argc < 2) {
            NSLog(@"Not enough arguments.");
            return -1;
        }

        [[_WKUserContentExtensionStore defaultStore] _removeAllContentExtensions];

        NSString *path = [NSString stringWithUTF8String:argv[1]];
        NSString *jsonString = [[NSString alloc] initWithContentsOfFile:path encoding:NSUTF8StringEncoding error:nil];

        __block bool doneCompiling = false;
        [[_WKUserContentExtensionStore defaultStore] compileContentExtensionForIdentifier:@"TestExtension" encodedContentExtension:jsonString completionHandler:^(_WKUserContentFilter *filter __attribute__((unused)), NSError *error) {
            if (error)
                printf("Failed to compile extension at %s with error %s", path.UTF8String , error.description.UTF8String);
            else
                printf("Succeeded in compiling extension at %s", path.UTF8String);

            doneCompiling = true;
        }];

        run(&doneCompiling);
    }

    return 0;
}
