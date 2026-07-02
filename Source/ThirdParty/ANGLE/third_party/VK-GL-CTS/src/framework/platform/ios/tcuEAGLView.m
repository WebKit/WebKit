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
 * \brief iOS Entry Point.
 *//*--------------------------------------------------------------------*/

#import "tcuEAGLView.h"

#import <QuartzCore/QuartzCore.h>

@implementation tcuEAGLView

+ (Class)layerClass
{
    // Override default layer.
    return [CAEAGLLayer class];
}

- (id)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    return self;
}

- (void)dealloc
{
    [super dealloc];
}

- (void)layoutSubviews
{
    // \todo [2013-10-28 pyry] How to handle this request?
}

- (CAEAGLLayer *)getEAGLLayer
{
    return (CAEAGLLayer *)self.layer;
}

@end
