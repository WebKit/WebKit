/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <getopt.h>
#import <WebCore/DrawDocumentPrivate.h>
#import <WebCore/DrawView.h>

#define COMMON_DIGEST_FOR_OPENSSL
#import <CommonCrypto/CommonDigest.h>

#define maxHeightOption     1
#define maxWidthOption      2

static void dumpRenderTree(const char *filename);
void dumpPixelTests(DrawDocument *document, NSString *svgPath);
static NSBitmapImageRep *getBitmapImageRepForSVGDocument(DrawDocument *document);
NSString *md5HashStringForBitmap(NSBitmapImageRep *bitmap);

/* global variables */
int pixelTests = 0;  // run the pixel comparison tests?
NSNumber *maxWidth = nil;
NSNumber *maxHeight = nil;
DrawView *view = nil;

int main(int argc, const char *argv[])
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    int ch = 0;

    // define the commandline options that this program accepts
    static struct option longopts[] = {
        { "pixel-tests",    no_argument,        &pixelTests,    1               },
        { "max-height",     required_argument,  NULL,           maxHeightOption },
        { "max-width",      required_argument,  NULL,           maxWidthOption  },
        { NULL,             0,                  NULL,           0               }
    };
    
    // parse the options that the program receives
    while ((ch = getopt_long(argc, (char * const *) argv, "", longopts, NULL)) != -1) {
        switch (ch) {
            case maxHeightOption:
                maxHeight = [NSNumber numberWithInt:[[NSString stringWithUTF8String:optarg] intValue]];
                break;
            case maxWidthOption:
                maxWidth = [NSNumber numberWithInt:[[NSString stringWithUTF8String:optarg] intValue]];
                break;
            case '?':   // unknown or ambiguous option
            case ':':   // missing argument
                exit(1);
                break;
        }
    }

    // remove the options that were parsed by getopt_long
    argc -= optind;
    argv += optind;

    // if we are running the pixel comparison tests, instantiate our DrawView
    if (pixelTests)
        view = [[DrawView alloc] initWithFrame:NSZeroRect];

    if (argc == 1 && strcmp(argv[0], "-") == 0) {
        char filenameBuffer[2048];

        while (fgets(filenameBuffer, sizeof(filenameBuffer), stdin)) {
            char *newLineCharacter = strchr(filenameBuffer, '\n');
            if (newLineCharacter) {
                *newLineCharacter = '\0';
            }
            dumpRenderTree(filenameBuffer);
            puts("#EOF");
            fflush(stdout);
        }
    } else {
        int i;
        for (i = 1; i != argc; ++i) {
            dumpRenderTree(argv[i]);
        }
    }

    [view release];
    [pool release];
    return 0;
}

static void dumpRenderTree(const char *filename)
{
    NSString *svgPath = [NSString stringWithUTF8String:filename];
    if (svgPath == nil) {
        fprintf(stderr, "can't parse filename as UTF-8\n");
        return;
    }
    
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    DrawDocument *document = [[DrawDocument alloc] initWithContentsOfFile:svgPath];
    
    NSString *result = [document renderTreeAsExternalRepresentation];
    if (!result)
        puts("error");
    else
        fputs([result UTF8String], stdout);
   
    // generate the image and perform pixel tests 
    if (pixelTests) {
        fputs("#EOF\n", stdout);
        dumpPixelTests(document, svgPath);
    }
    
    [document release];
    [pool release];
}

NSString *md5HashStringForBitmap(NSBitmapImageRep *bitmap)
{
    MD5_CTX md5Context;
    unsigned char hash[16];

    MD5_Init(&md5Context);
    MD5_Update(&md5Context, [bitmap bitmapData], [bitmap bytesPerPlane]);
    MD5_Final(hash, &md5Context);
    
    char hex[33] = "";
    for (int i = 0; i < 16; i++) {
       snprintf(hex, 33, "%s%02x", hex, hash[i]);
    }

    return [NSString stringWithUTF8String:hex];
}

void dumpPixelTests(DrawDocument *document, NSString *svgPath)
{
    // Creates an image from the document object
    NSBitmapImageRep *actualBitmap = getBitmapImageRepForSVGDocument(document);

    // lookup the expected hash value for that image
    NSString *baseTestPath = [svgPath stringByDeletingPathExtension];
    NSString *baselineHashPath = [baseTestPath stringByAppendingString:@"-expected.checksum"];
    NSString *baselineHash = [NSString stringWithContentsOfFile:baselineHashPath encoding:NSUTF8StringEncoding error:nil];

    // compute the hash for the rendered image, print both.
    NSString *actualHash = md5HashStringForBitmap(actualBitmap);
    fprintf(stdout,"ActualHash: %s\n", [actualHash UTF8String]);
    fprintf(stdout,"BaselineHash: %s\n", [baselineHash UTF8String]);
    
    // if the hashes don't match, then perform more pixel tests
    if ([baselineHash isEqualToString:actualHash] == NO) {
        // send the PNG compressed data through STDOUT
        NSData *svgPNGData = [actualBitmap representationUsingType:NSPNGFileType properties:nil];
        fprintf(stdout, "Content-length: %d\n", [svgPNGData length]);
        fwrite([svgPNGData bytes], [svgPNGData length], 1, stdout);
    }
}

static NSSize constrainSizeToMaximum(NSSize boundsSize)
{
    // Limit the maximum size and default the size to 500x500 if not specified
    if (maxWidth && boundsSize.width > [maxWidth intValue]) {
        // resize the height so that the aspect ratio remains the same
        boundsSize.height = (boundsSize.height / boundsSize.width) * [maxWidth intValue];
        boundsSize.width = [maxWidth intValue];
    }
    else if (boundsSize.width <= 0)
        boundsSize.width = 500;
        
    if (maxHeight && boundsSize.height > [maxHeight intValue]) {
        // resize the width so that the aspect ratio reamins the same
        boundsSize.width = (boundsSize.width / boundsSize.height) * [maxHeight intValue];
        boundsSize.height = [maxHeight intValue];
    }
    else if (boundsSize.height <= 0)
        boundsSize.height = 500;
    return boundsSize;
}

static NSBitmapImageRep *getBitmapImageRepForSVGDocument(DrawDocument *document)
{
    [view setDocument:document];
    [view sizeToFitViewBox];
    NSSize boundsSize = constrainSizeToMaximum([view bounds].size);
    [view setFrameSize:boundsSize];
    
    // tell that view to render
    NSBitmapImageRep *imageRep = [view bitmapImageRepForCachingDisplayInRect:[view bounds]];
    [view cacheDisplayInRect:[view bounds] toBitmapImageRep:imageRep];

    return imageRep;
}
