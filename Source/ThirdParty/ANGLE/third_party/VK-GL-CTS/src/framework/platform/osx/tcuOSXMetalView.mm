/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2018 The Android Open Source Project
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
 * \brief VK_EXT_metal_surface compatible view
 *//*--------------------------------------------------------------------*/

#include "tcuOSXMetalView.hpp"

#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>

@interface NativeMetalView : NSView
@end

@implementation NativeMetalView
- (id)initWithFrame:(NSRect)frame
{
    if (self = [super initWithFrame:frame])
    {
        // Make this a layer-backed view
        self.wantsLayer = YES;
    }
    return self;
}

// Callback to create the backing metal layer
- (CALayer *)makeBackingLayer
{
    return [CAMetalLayer layer];
}
@end

namespace tcu
{
namespace osx
{
MetalView::MetalView(int width, int height)
    : m_view([[NativeMetalView alloc]
          initWithFrame:NSMakeRect(0, 0, width, height)])
{
}

void MetalView::setSize(int width, int height)
{
    [(NativeMetalView *)m_view setFrame:NSMakeRect(0, 0, width, height)];
}

vk::pt::CAMetalLayer MetalView::getLayer() const
{
    return vk::pt::CAMetalLayer((void *)[(NativeMetalView *)m_view layer]);
}

MetalView::~MetalView() { [(NativeMetalView *)m_view release]; }
} // namespace osx
} // namespace tcu
