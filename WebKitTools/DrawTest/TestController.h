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

#import <Cocoa/Cocoa.h>

@class DrawView;
@class SVGTest;
@class TestViewerSplitView;

@interface TestController : NSObject {
    IBOutlet NSPanel *_testPanel;
    IBOutlet NSWindow *_testWindow;
    IBOutlet TestViewerSplitView *_splitView;

    IBOutlet NSArrayController *_testsArrayController;
    IBOutlet NSPopUpButton *_parentDirectoryPopup;
    IBOutlet NSTableView *_testsTableView;
    
    IBOutlet NSWindow *_compositeWindow;
    IBOutlet NSImageView *_compositeImageView;
    
@private
    NSString *_currentPath;
    NSArray *_tests;
    SVGTest *_selectedTest;
    
    DrawView *_drawView;
    NSImageView *_imageView;
}

+ (id)sharedController;

- (IBAction)showTestsPanel:(id)sender;
- (IBAction)showTestWindow:(id)sender;
- (IBAction)showCompositeWindow:(id)sender;

- (IBAction)browse:(id)sender;
- (IBAction)jumpToParentDirectory:(id)sender;
- (IBAction)openTestViewerForSelection:(id)sender;
- (IBAction)openSourceForSelection:(id)sender;
- (IBAction)openSelectionInViewer:(id)sender;
- (IBAction)toggleViewersScaleRule:(id)sender;

- (NSArray *)tests;
- (NSString *)currentPath;
- (void)setCurrentPath:(NSString *)newPath;
- (NSArray *)directoryHierarchy;

@end
