/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import <WebCore+SVG/DrawDocumentPrivate.h>
#import <WebCore+SVG/DrawView.h>

extern const double svg2pngVersionNumber;
extern const unsigned char svg2pngVersionString[];

void usage(const char *name)
{
    printf("Usage: %s [options] file[s]\n", name);
    printf("Options:\n");
    printf("  --version\t\t\tPrints full version (%.1f) information and exits.\n", svg2pngVersionNumber);
    printf("  --output <outputfile> dump a png copy of a single svg input file to the named path\n");
    printf("  --suffix <suffix> append <suffix> to each output file e.g. <inputname><suffix>.png\n");
    printf("When --output is not specified, output files are created in the same directory\n");
    printf("as the original source files e.g. <source directory>/<filename><suffix>.png");
    exit(1);
};

void outputPNG(NSString *svgPath, NSString *pngPath)
{
    // load the given document
    DrawDocument *document = [[DrawDocument alloc] initWithContentsOfFile:svgPath];
    
    // create a view
    DrawView *view = [[DrawView alloc] initWithFrame:NSZeroRect]; // could be static...
    [view setDocument:document];
    [view sizeToFitViewBox];
    
    // My maximum 500x500 size hack:
    NSSize boundsSize = [view bounds].size;
    if (boundsSize.width > 500)
        boundsSize.width = 500;
    if (boundsSize.height > 500)
        boundsSize.height = 500;
    [view setFrameSize:boundsSize];
    
    // tell that view to render
    NSBitmapImageRep *imageRep = [view bitmapImageRepForCachingDisplayInRect:[view bounds]];
    [view cacheDisplayInRect:[view bounds] toBitmapImageRep:imageRep];
    
    // output the resulting image
    NSData *pngData = [imageRep representationUsingType:NSPNGFileType properties:nil];
    [pngData writeToFile:svgPath atomically:YES];
    
    [view release]; // should be singleton.
    [document release];
}

int main (int argc, const char *argv[])
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    // deal with args
    NSMutableArray *svgs = [NSMutableArray array];
    const char *name = [[[NSString stringWithUTF8String:argv[0]] lastPathComponent] UTF8String];
    NSString *explicitOutputPath = nil;
    NSString *outputSuffix = nil;
    
    for(int x = 1; x < argc; x++) {
        const char *arg = argv[x];
        NSString *argString = [NSString stringWithUTF8String:arg];
        
        if ([argString hasPrefix:@"--"]) { // this is an option
            if ([argString isEqualToString:@"--help"])
                usage(name);
            else if ([argString isEqualToString:@"--version"]) {
                printf((char *)svg2pngVersionString);
                exit(0);
            } else if ([argString isEqualToString:@"--output"]) {
                if (x + 1 < argc) {
                    explicitOutputPath = [NSString stringWithUTF8String:argv[x+1]];
                    if ([explicitOutputPath hasPrefix:@"--"])
                        NSLog(@"WARNING: did you really mean to output to: %@  Or did you just forget the --output <filename> argument?", explicitOutputPath);
                    x++; // to skip the next arg
                } else {
                    NSLog(@"--output requires an argument.");
                    usage(name);
                }
            }  else if ([argString isEqualToString:@"--suffix"]) {
                if (x + 1 < argc) {
                    outputSuffix = [NSString stringWithUTF8String:argv[x+1]];
                    x++; // to skip the next arg
                } else {
                    NSLog(@"--suffix requires an argument.");
                    usage(name);
                }
            } else {
                printf("Unrecognized option %@\n", argString);
                usage(name);
            }
        } else {
            if (![[NSFileManager defaultManager] fileExistsAtPath:[argString stringByStandardizingPath]]) {
                printf("SVG does not exist at path: %@\n", argString);
                exit(1);
            }
            [svgs addObject:argString];
        }
    }
    
    if (![svgs count]) {
        printf("No svgs specified.\n");
        usage(name);
    }
    
    if ( ([svgs count] > 1) && explicitOutputPath) {
        printf("--output name specifed, yet more than one input file was given!\n\n");
        usage(name);
    }
    
    NSString *svgPath = nil;
    foreacharray(svgPath, svgs) {
        NSString *pngPath = nil;
        if (explicitOutputPath)
            pngPath = explicitOutputPath;
        else {
            pngPath = [svgPath stringByDeletingPathExtension];
            if (outputSuffix)
                pngPath = [pngPath stringByAppendingString:outputSuffix];
            pngPath = [pngPath stringByAppendingPathExtension:@"png"];
        }
        outputPNG(svgPath, pngPath);
    }
    
    [pool drain];
    
    return 0;
}


