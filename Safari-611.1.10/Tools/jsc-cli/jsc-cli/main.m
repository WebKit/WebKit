/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "CLIInstance.h"

static void printUsage()
{
    fprintf(stderr, "Usage: jsc-cli file [program-args]\n");
    fprintf(stderr, "    jsc-cli is a command line interface for executing JavaScript code.\n");
    fprintf(stderr, "    file should be a JavaScript source file. Any arguments following the\n");
    fprintf(stderr, "    source file are the arguments to the program itself.\n");
}

int main(int argc, const char * argv[])
{
    @autoreleasepool {
        CLIInstance *instance = [[CLIInstance alloc] init];
        
        int offset = 1;
        if (argc > 1) {
            NSString *filename = [NSString stringWithCString:argv[1] encoding:NSUTF8StringEncoding];
            BOOL directory;
            if (![[NSFileManager defaultManager] fileExistsAtPath:filename isDirectory:&directory]) {
                fprintf(stderr, "Error: %s does not exist.\n", argv[1]);
                printUsage();
                exit(-1);
            }
            
            if (directory) {
                fprintf(stderr, "Error: %s is a directory.\n", argv[1]);
                printUsage();
                exit(-1);
            }
            
            [instance loadFile:filename];
            offset = 2;
        }
        [instance didReceiveArguments:argv atOffset:offset withLength:argc];

        [instance run];
    }
    return 0;
}

