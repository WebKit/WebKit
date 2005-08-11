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

#import "TestController.h"
#import "SVGTest.h"
#import "TestViewerSplitView.h"
#import "ScalingImageView.h"

#import <WebCore+SVG/DrawView.h>

@interface NSArray (TestControllerAdditions)
- (id)firstObject;
@end

@implementation NSArray (TestControllerAdditions)
- (id)firstObject
{
    if ([self count])
        return [self objectAtIndex:0];
    return nil;
}
@end

static TestController *__sharedInstance = nil;

@implementation TestController

- (id)init
{
    if (self = [super init]) {
        NSString *path = [[NSUserDefaults standardUserDefaults] objectForKey:@"TestDirectory"];
        BOOL isDirectory = NO;
        if (![[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&isDirectory] || !isDirectory) {
            path = [@"~" stringByStandardizingPath];
        }
        [self setCurrentPath:path];
    }
    return self;
}

+ (void)initialize
{
    [self setKeys:[NSArray arrayWithObject:@"currentPath"] triggerChangeNotificationsForDependentKey:@"directoryHierarchy"];
    [self setKeys:[NSArray arrayWithObject:@"currentPath"] triggerChangeNotificationsForDependentKey:@"tests"];
}

+ (id)sharedController
{
    if (!__sharedInstance) {
        __sharedInstance = [[self alloc] init];
    }
    return __sharedInstance;
}

- (void)loadNibIfNecessary
{
    if (!_testPanel) {
        [NSBundle loadNibNamed:@"TestViewer" owner:self];
    }
}

- (void)awakeFromNib
{
    [_testsTableView setTarget:self];
    [_testsTableView setDoubleAction:@selector(openTestViewerForSelection:)];
    _drawView = [[DrawView alloc] initWithFrame:NSZeroRect];
    _imageView = [[ScalingImageView alloc] initWithFrame:NSZeroRect];
    [_splitView addSubview:_drawView];
    [_splitView addSubview:_imageView];
}

- (IBAction)showTestsPanel:(id)sender
{
    [self loadNibIfNecessary];
    [_testPanel makeKeyAndOrderFront:sender];
}

- (IBAction)showTestWindow:(id)sender
{
    [self loadNibIfNecessary];
    [_testWindow makeKeyAndOrderFront:sender];
}

- (IBAction)showCompositeWindow:(id)sender
{
    [self loadNibIfNecessary];
    NSLog(@"showCompositeWindow: %@", _compositeWindow);
    [_compositeWindow makeKeyAndOrderFront:sender];
}

- (IBAction)browse:(id)sender
{
    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    [openPanel setCanChooseDirectories:YES];
    [openPanel setCanChooseFiles:NO];
    [openPanel beginSheetForDirectory:nil file:nil modalForWindow:_testPanel modalDelegate:self didEndSelector:@selector(openPanelDidEnd:returnCode:contextInfo:) contextInfo:NULL];
}

- (void)openPanelDidEnd:(NSOpenPanel *)openPanel returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
    if (returnCode == NSOKButton) {
        NSArray *folders = [openPanel filenames];
        NSString *selectedFolder = [folders firstObject];
        [self setCurrentPath:selectedFolder];
    }
}

- (IBAction)jumpToParentDirectory:(id)sender
{
    int index = [_parentDirectoryPopup indexOfSelectedItem];
    NSArray *components = [_currentPath pathComponents];
    NSArray *newComponents = [components subarrayWithRange:NSMakeRange(0, [components count] - index)];
    NSString *newPath = [NSString pathWithComponents:newComponents];
    [self setCurrentPath:newPath];
}

- (void)setSelectedTest:(SVGTest *)selectedTest
{
    id oldTest = _selectedTest;
    _selectedTest = [selectedTest retain];
    [oldTest release];
    
    if ([_testWindow isVisible]) {
        [_testWindow setTitle:[NSString stringWithFormat:@"Test Viewer - %@", [_selectedTest name]]]; 
        [_drawView setDocument:[_selectedTest svgDocument]];
        [_imageView setImage:[_selectedTest image]];
        if ([_compositeWindow isVisible])
            [_compositeImageView setImage:[_selectedTest compositeImage]];
    }
}

- (void)tableViewSelectionDidChange:(NSNotification *)aNotification
{
    [self setSelectedTest:[[_testsArrayController selectedObjects] firstObject]];
}

- (IBAction)openTestViewerForSelection:(id)sender
{
    [self showTestWindow:sender];
    [_drawView setDocument:[_selectedTest svgDocument]];
    [_imageView setImage:[_selectedTest image]];
}

- (IBAction)openSourceForSelection:(id)sender
{
    [[NSWorkspace sharedWorkspace] openFile:[_selectedTest svgPath] withApplication:@"TextEdit"];
}

- (IBAction)openSelectionInViewer:(id)sender
{
    [[NSWorkspace sharedWorkspace] openFile:[_selectedTest svgPath]];
}

- (NSString *)imagePathForSVGPath:(NSString *)svgPath
{
    // eventually this code will build an array instead...
    
    NSString *currentDirectory = [self currentPath];
    NSString *parentDirectory = [currentDirectory stringByDeletingLastPathComponent];
    
    NSString *testName = [[svgPath lastPathComponent] stringByDeletingPathExtension];
    NSString *imageName, *imageDirectory, *imagePath;
    
    // first look in ../png/test.png -- SVG 1.1 baselines
    // The SVG 1.1 spec has various different pngs, we should allow the
    // tester to choose...
    imageName = [[@"full-" stringByAppendingString:testName] stringByAppendingPathExtension:@"png"];
    imageDirectory = [parentDirectory stringByAppendingPathComponent:@"png"];
    imagePath = [imageDirectory stringByAppendingPathComponent:imageName];
    if ([[NSFileManager defaultManager] fileExistsAtPath:imagePath]) return imagePath;
    
    // then look for ../name.png -- openclipart.org
    imageName = [testName stringByAppendingPathExtension:@"png"];
    imageDirectory = parentDirectory;
    imagePath = [imageDirectory stringByAppendingPathComponent:imageName];
    if ([[NSFileManager defaultManager] fileExistsAtPath:imagePath]) return imagePath;
    
    // then look for ./name-w3c.png -- WebCore tests
    imageName = [[testName stringByAppendingString:@"-w3c"] stringByAppendingPathExtension:@"png"];
    imageDirectory = currentDirectory;
    imagePath = [imageDirectory stringByAppendingPathComponent:imageName];
    if ([[NSFileManager defaultManager] fileExistsAtPath:imagePath]) return imagePath;
    
    // finally try name-baseline.png -- ksvg regression baselines
    imageName = [[testName stringByAppendingString:@"-baseline"] stringByAppendingPathExtension:@"png"];
    imageDirectory = currentDirectory;
    imagePath = [imageDirectory stringByAppendingPathComponent:imageName];
    if ([[NSFileManager defaultManager] fileExistsAtPath:imagePath]) return imagePath;
    
    return nil;
}

- (NSArray *)tests
{
    if (!_tests) {
        NSMutableArray *newTests = [[NSMutableArray alloc] init];
        NSArray *files = [[NSFileManager defaultManager] directoryContentsAtPath:[self currentPath]];
        NSString *file = nil;
        foreacharray(file, files) {
            if ([[file pathExtension] isEqualToString:@"svg"]) {
                NSString *svgPath = [[self currentPath] stringByAppendingPathComponent:file];
                NSString *imagePath = [self imagePathForSVGPath:svgPath];
                [newTests addObject:[SVGTest testWithSVGPath:svgPath imagePath:imagePath]];
            }
        }
        [self setValue:newTests forKey:@"tests"];
    }
    return _tests;
}

- (NSArray *)directoryHierarchy
{
    // A hackish way to reverse an array.
    return [[[_currentPath pathComponents] reverseObjectEnumerator] allObjects];
}

- (NSString *)currentPath
{
    return _currentPath;
}

- (void)setCurrentPath:(NSString *)newPath
{
    if (![newPath isEqualToString:_currentPath]) {
        [_currentPath release];
        _currentPath = [newPath copy];
        [self setValue:nil forKey:@"tests"];
    }
    
    [[NSUserDefaults standardUserDefaults] setObject:_currentPath forKey:@"TestDirectory"];
}

- (IBAction)toggleViewersScaleRule:(id)sender
{
    if ([_drawView imageScaling] == NSScaleProportionally) {
        [_drawView setImageScaling:NSScaleNone];
        [_imageView setImageScaling:NSScaleNone];
    } else {
        [_drawView setImageScaling:NSScaleProportionally];
        [_imageView setImageScaling:NSScaleProportionally];
    }
}

@end
