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
#import "ImageDiff.h"

extern const double svg2pngVersionNumber;
extern const unsigned char svg2pngVersionString[];

/* global variables */
BOOL isMaxHeightSpecified = NO;
BOOL isMaxWidthSpecified = NO;
int maxWidth = 0;
int maxHeight = 0;

void usage(const char *name)
{
    printf("Usage: %s [options] file[s]\n", name);
    printf("Options:\n");
    printf("  --baseline-dir <directory>\tthe directory where the baselines are stored\n");
    printf("  --baseline-suffix <suffix>\tthe suffix on the reference version of a picture\n");
    printf("  --max-width <int>\t\tsets the width of the output png\n");
    printf("  --max-height <int>\t\tsets the height of the output png\n");
    printf("  --output <outputfile>\t\tdump a png copy of a single svg input file\n\t\t\t\tto the named path\n");
    printf("  --output-dir <directory>\tthe directory where all files are output to\n");
    printf("  --suffix <suffix>\t\tappend <suffix> to each output file\n\t\t\t\te.g. <inputname><suffix>.png\n\n");
    printf("  --version\t\t\tPrints full version (%.1f) information and exits\n", svg2pngVersionNumber);
    printf("When --output is not specified, output files are created in the same directory\n");
    printf("as the original source files e.g. <source directory>/<filename><suffix>.png\n");
    printf("\nWhen --baseline-suffix is specified, svg2png will look for a PNG file that\n");
    printf("contains an example of what the SVG should look like when rendered. It will\n");
    printf("create a <filename><suffix>-diff.png file to show these differences.\n\n"); 
    exit(1);
};

NSBitmapImageRep *getBitmapForSVG(NSString *svgPath)
{
    
    // load the given document
    DrawDocument *document = [[DrawDocument alloc] initWithContentsOfFile:svgPath];
    
    // create a view
    DrawView *view = [[DrawView alloc] initWithFrame:NSZeroRect]; // could be static...
    [view setDocument:document];
    [view sizeToFitViewBox];
    
    // Limit the maximum size, and default the size to 500x500 if not specified
    NSSize boundsSize = [view bounds].size;
    if (isMaxWidthSpecified && boundsSize.width > maxWidth) {
        // resize the height so that the aspect ratio remains the same
        boundsSize.height = (boundsSize.height / boundsSize.width) * maxWidth;
        boundsSize.width = maxWidth;
    }
    else if (boundsSize.width <= 0)
        boundsSize.width = 500;
        
    if (isMaxHeightSpecified && boundsSize.height > maxHeight) {
        // resize the width so that the aspect ratio reamins the same
        boundsSize.width = (boundsSize.width / boundsSize.height) * maxHeight;
        boundsSize.height = maxHeight;
    }
    else if (boundsSize.height <= 0)
        boundsSize.height = 500;
    
    [view setFrameSize:boundsSize];
    
    // tell that view to render
    NSBitmapImageRep *imageRep = [view bitmapImageRepForCachingDisplayInRect:[view bounds]];
    [view cacheDisplayInRect:[view bounds] toBitmapImageRep:imageRep];
    
    [view release]; // should be singleton.
    [document release];
    
    return imageRep;
}

int main (int argc, const char *argv[])
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    // deal with args
    NSMutableArray *svgs = [NSMutableArray array];
    const char *name = [[[NSString stringWithUTF8String:argv[0]] lastPathComponent] UTF8String];
    NSString *explicitOutputFile = nil;
    NSString *explicitOutputDirectory = nil;
    NSString *outputSuffix = nil;
    NSString *baselineSuffix = nil;
    NSString *explicitBaselineDirectory = nil;
    
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
                    explicitOutputFile = [NSString stringWithUTF8String:argv[x+1]];
                    if ([explicitOutputFile hasPrefix:@"--"])
                        printf("WARNING: did you really mean to output to: %s  Or did you just forget the --output <filename> argument?\n", [explicitOutputFile UTF8String]);
                    x++; // to skip the next arg
                } else {
                    printf("--output requires an argument.\n");
                    usage(name);
                }
            } else if ([argString isEqualToString:@"--output-dir"]) {
                if (x + 1 < argc) {
                    explicitOutputDirectory = [NSString stringWithUTF8String:argv[x+1]];
                    if ([explicitOutputDirectory hasPrefix:@"--"])
                        printf("WARNING: did you really mean to output to: %s  Or did you just forget the --output <filename> argument?\n", [explicitOutputDirectory UTF8String]);
                    x++; // to skip the next arg
                } else {
                    printf("--output-dir requires an argument.\n");
                    usage(name);
                }
            }  else if ([argString isEqualToString:@"--suffix"]) {
                if (x + 1 < argc) {
                    outputSuffix = [NSString stringWithUTF8String:argv[x+1]];
                    x++; // to skip the next arg
                } else {
                    printf("--suffix requires an argument.\n");
                    usage(name);
                }
            } else if ([argString isEqualToString:@"--max-width"]) {
                if (x + 1 < argc) {
                    maxWidth = [[NSString stringWithUTF8String:argv[x+1]] intValue];
                    if (maxWidth <= 0) {
                        printf("--max-width must be an integer > 0\n");
                        usage(name);
                    }
                    isMaxWidthSpecified = YES;
                    x++; // skip to the next arg
                } else {
                    printf("--max-width requires an integer argument.\n");
                    usage(name);
                }
            } else if ([argString isEqualToString:@"--max-height"]) {
                if (x + 1 < argc) {
                    maxHeight = [[NSString stringWithUTF8String:argv[x+1]] intValue];
                    if (maxHeight <= 0) {
                        printf("--max-height must be an integer > 0\n");
                        usage(name);
                    }
                    isMaxHeightSpecified = YES;
                    x++; // skip to the next arg
                } else {
                    printf("--max-height requires an integer > 0\n");
                    usage(name);
                }
            } else if ([argString isEqualToString:@"--baseline-suffix"]) {
                if(x + 1 < argc) {
                    baselineSuffix = [NSString stringWithUTF8String:argv[x+1]];
                    x++; // skip to the next arg
                } else {
                    printf("--baseline-suffix requires the suffix for the reference PNGs.\n");
                    usage(name);
                }
            } else if ([argString isEqualToString:@"--baseline-dir"]) {
                if(x + 1 < argc) {
                    explicitBaselineDirectory = [NSString stringWithUTF8String:argv[x+1]];
                    x++; // skip to the next arg
                } else {
                    printf("--baseline-dir requires the suffix for the reference PNGs.\n");
                    usage(name);
                }
            } else {
                printf("Unrecognized option %s\n", [argString UTF8String]);
                usage(name);
            }
        } else {
            if (![[NSFileManager defaultManager] fileExistsAtPath:[argString stringByStandardizingPath]]) {
                printf("SVG does not exist at path: %s\n", [argString UTF8String]);
                exit(1);
            }
            [svgs addObject:argString];
        }
    }
    
    if (![svgs count]) {
        printf("No svgs specified.\n");
        usage(name);
    }
    
    if ( ([svgs count] > 1) && explicitOutputFile) {
        printf("--output name specifed, yet more than one input file was given!\n\n");
        usage(name);
    }
    
    
    NSString *svgPath = nil;
    foreacharray(svgPath, svgs) {
        NSBitmapImageRep *svgBitmap = nil;
        NSString *svgPNGPath = nil;
        
        if (explicitOutputFile)
            svgPNGPath = explicitOutputFile;
        else {
            if (explicitOutputDirectory)
                svgPNGPath = [[explicitOutputDirectory stringByAppendingPathComponent:[svgPath lastPathComponent]] stringByDeletingPathExtension];
            else     
                svgPNGPath = [svgPath stringByDeletingPathExtension];
            
            if (outputSuffix)
                svgPNGPath = [svgPNGPath stringByAppendingString:outputSuffix];
            
            svgPNGPath = [svgPNGPath stringByAppendingPathExtension:@"png"];
        }
        
        // generate the SVG, save the bitmap data for the diff
        svgBitmap = getBitmapForSVG(svgPath);
        
        // output the resulting SVG to a PNG file
        NSData *svgPNGData = [svgBitmap representationUsingType:NSPNGFileType properties:nil];
        [svgPNGData writeToFile:svgPNGPath atomically:YES];
        
        if (baselineSuffix || explicitBaselineDirectory) {
            NSString *diffImagePath = [[[svgPNGPath stringByDeletingPathExtension]
                stringByAppendingString:@"-diff"]
                stringByAppendingPathExtension:@"png"];
            
            NSString *animatedGIFPath = [[diffImagePath stringByDeletingPathExtension]
                stringByAppendingPathExtension:@"gif"];

            NSString *baselineImagePath = nil;
            
            // if set, look for baselines in a seperate directory
            if (explicitBaselineDirectory)
                baselineImagePath = [[explicitBaselineDirectory 
                    stringByAppendingPathComponent:[svgPath lastPathComponent]] stringByDeletingPathExtension];
            else
                baselineImagePath = [svgPath stringByDeletingPathExtension];
            
            // if baselines have a special suffix, append that to the path
            if (baselineSuffix)
                baselineImagePath = [baselineImagePath stringByAppendingString:baselineSuffix];
            
            baselineImagePath = [baselineImagePath stringByAppendingPathExtension:@"png"];

            // compare
            NSBitmapImageRep *referenceBitmap = [NSBitmapImageRep imageRepWithContentsOfURL:[NSURL fileURLWithPath:baselineImagePath]];
            NSBitmapImageRep *diffBitmap = getDifferenceBitmap(svgBitmap, referenceBitmap);

            // output the resulting SVG to a PNG file
            NSData *diffPNGData = [diffBitmap representationUsingType:NSPNGFileType properties:nil];
            [diffPNGData writeToFile:diffImagePath atomically:YES];

            // save the reference and the svg images in an animated GIF
            saveAnimatedGIFToFile(svgBitmap, referenceBitmap, animatedGIFPath);
            
            float percentage = computePercentageDifferent(diffBitmap);
            printf("%s\t%3.3f%%\t%s\n", [svgPath UTF8String], percentage, (percentage > 1 ? "Failed." : "Passed."));
            
            [diffBitmap release];
        }
    }
    
    [pool drain];
    
    return 0;
}
