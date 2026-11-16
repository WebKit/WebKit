/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief iOS View Controller.
 *//*--------------------------------------------------------------------*/

#import <QuartzCore/QuartzCore.h>

#import "tcuEAGLView.h"
#import "tcuIOSViewController.h"

#include "qpDebugOut.h"

@interface tcuIOSViewController ()
@property(nonatomic, assign) CADisplayLink *displayLink;
@end

@implementation tcuIOSViewController

@synthesize displayLink;

- (void)loadView
{
    tcuEAGLView *view = [[tcuEAGLView alloc]
        initWithFrame:[UIScreen mainScreen].applicationFrame];
    self.view = view;
    [view release];
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    isIterating = FALSE;
    self.displayLink = nil;
    app = tcuIOSApp_create(self.view);
}

- (void)dealloc
{
    [super dealloc];
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
}

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];
}

- (void)viewWillDisappear:(BOOL)animated
{
    [super viewWillDisappear:animated];
}

- (void)viewDidUnload
{
    [super viewDidUnload];
}

- (void)startTestIteration
{
    if (!isIterating)
    {
        DE_ASSERT(self.displayLink == nil);

        // Obtain display link.
        self.displayLink =
            [[UIScreen mainScreen] displayLinkWithTarget:self
                                                selector:@selector(iterate)];
        [self.displayLink setFrameInterval:1];
        [self.displayLink addToRunLoop:[NSRunLoop currentRunLoop]
                               forMode:NSDefaultRunLoopMode];

        isIterating = TRUE;
    }
}

- (void)stopTestIteration
{
    if (isIterating)
    {
        isIterating = FALSE;
        [self.displayLink invalidate];
        self.displayLink = nil;
    }
}

- (void)iterate
{
    if (isIterating)
    {
        bool result = tcuIOSApp_iterate(app);

        if (!result)
        {
            [self stopTestIteration];
            qpDief("Fatal error occurred in test execution, killing process.");
        }
    }
}

@end
