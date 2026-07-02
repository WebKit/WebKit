/*
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "Cursor.h"

#if HAVE(NSCURSOR)

#import "ImageAdapter.h"
#import "NativeImage.h"
#import <AppKit/NSCursor.h>
#import <objc/runtime.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/StdLibExtras.h>

#if HAVE(HISERVICES)
#import <pal/spi/mac/HIServicesSPI.h>
#endif

#if HAVE(HISERVICES)

@interface WebCustomCursor : NSCursor

- (instancetype)initWithType:(CoreCursorType)cursorType;

@property (nonatomic) CoreCursorType cursorType;

@end

@implementation WebCustomCursor

- (instancetype)initWithType:(CoreCursorType)cursorType
{
    self = [super init];
    if (!self)
        return nil;

    _cursorType = cursorType;
    return self;
}

- (NSInteger)_coreCursorType
{
    return static_cast<NSInteger>(_cursorType);
}

@end

#endif

namespace WebCore {

#if HAVE(HISERVICES)

static RetainPtr<NSCursor> cursor(ASCIILiteral name)
{
    auto cursorTypeFromName = [](ASCIILiteral name) {
        if (name == "BusyButClickable"_s)
            return kCoreCursorBusyButClickable;
        if (name == "MakeAlias"_s)
            return kCoreCursorMakeAlias;
        if (name == "Move"_s)
            return kCoreCursorWindowMove;
        if (name == "ResizeEast"_s)
            return kCoreCursorWindowResizeEast;
        if (name == "ResizeEastWest"_s)
            return kCoreCursorWindowResizeEastWest;
        if (name == "ResizeNorth"_s)
            return kCoreCursorWindowResizeNorth;
        if (name == "ResizeNorthSouth"_s)
            return kCoreCursorWindowResizeNorthSouth;
        if (name == "ResizeNortheast"_s)
            return kCoreCursorWindowResizeNorthEast;
        if (name == "ResizeNortheastSouthwest"_s)
            return kCoreCursorWindowResizeNorthEastSouthWest;
        if (name == "ResizeNorthwest"_s)
            return kCoreCursorWindowResizeNorthWest;
        if (name == "ResizeNorthwestSoutheast"_s)
            return kCoreCursorWindowResizeNorthWestSouthEast;
        if (name == "ResizeSouth"_s)
            return kCoreCursorWindowResizeSouth;
        if (name == "ResizeSoutheast"_s)
            return kCoreCursorWindowResizeSouthEast;
        if (name == "ResizeSouthwest"_s)
            return kCoreCursorWindowResizeSouthWest;
        if (name == "ResizeWest"_s)
            return kCoreCursorWindowResizeWest;
        if (name == "Cell"_s)
            return kCoreCursorCell;
        if (name == "Help"_s)
            return kCoreCursorHelp;
        if (name == "ZoomIn"_s)
            return kCoreCursorZoomIn;
        if (name == "ZoomOut"_s)
            return kCoreCursorZoomOut;

        ASSERT_NOT_REACHED();
        return kCoreCursorFirstCursor;
    };

    return adoptNS([[WebCustomCursor alloc] initWithType:cursorTypeFromName(name)]);
}

#else

static RetainPtr<NSCursor> cursor(ASCIILiteral)
{
    return [NSCursor arrowCursor];
}

#endif // HAVE(HISERVICES)

// Simple NSCursor calls shouldn't need protection,
// but creating a cursor with a bad image might throw.

#if ENABLE(CUSTOM_CURSOR_SUPPORT)
#if ENABLE(MOUSE_CURSOR_SCALE)
static RetainPtr<NSCursor> createCustomCursor(Image* image, const IntPoint& hotSpot, float scale)
#else
static RetainPtr<NSCursor> createCustomCursor(Image* image, const IntPoint& hotSpot)
#endif
{
    // FIXME: The cursor won't animate.  Not sure if that's a big deal.
    auto nsImage = image->adapter().snapshotNSImage();
    if (!nsImage)
        return nullptr;
    BEGIN_BLOCK_OBJC_EXCEPTIONS

#if ENABLE(MOUSE_CURSOR_SCALE)
    NSSize size = NSMakeSize(image->width() / scale, image->height() / scale);
    NSSize expandedSize = NSMakeSize(ceil(size.width), ceil(size.height));

    // Pad the image with transparent pixels so it has an integer boundary.
    if (size.width != expandedSize.width || size.height != expandedSize.height) {
        RetainPtr<NSImage> expandedImage = adoptNS([[NSImage alloc] initWithSize:expandedSize]);
        NSRect toRect = NSMakeRect(0, expandedSize.height - size.height, size.width, size.height);
        NSRect fromRect = NSMakeRect(0, 0, image->width(), image->height());

        [expandedImage lockFocus];
        [nsImage drawInRect:toRect fromRect:fromRect operation:NSCompositingOperationSourceOver fraction:1];
        [expandedImage unlockFocus];

        return adoptNS([[NSCursor alloc] initWithImage:expandedImage.get() hotSpot:hotSpot]);
    }

    // Scale the image and its representation to match retina resolution.
    [nsImage setSize:expandedSize];
    [[retainPtr([nsImage representations]) objectAtIndex:0] setSize:expandedSize];
#endif

    return adoptNS([[NSCursor alloc] initWithImage:nsImage.get() hotSpot:hotSpot]);
    END_BLOCK_OBJC_EXCEPTIONS
    return nullptr;
}
#endif // ENABLE(CUSTOM_CURSOR_SUPPORT)

void Cursor::ensurePlatformCursor() const
{
    if (m_platformCursor)
        return;

    switch (m_type) {
    case Type::Pointer:
        m_platformCursor = [NSCursor arrowCursor];
        break;

    case Type::Cross:
        m_platformCursor = [NSCursor crosshairCursor];
        break;

    case Type::Hand:
        m_platformCursor = [NSCursor pointingHandCursor];
        break;

    case Type::IBeam:
        m_platformCursor = [NSCursor IBeamCursor];
        break;

    case Type::Wait:
        m_platformCursor = cursor("BusyButClickable"_s);
        break;

    case Type::Help:
        m_platformCursor = cursor("Help"_s);
        break;

    case Type::Move:
    case Type::MiddlePanning:
        m_platformCursor = cursor("Move"_s);
        break;

    case Type::EastResize:
    case Type::EastPanning:
        m_platformCursor = cursor("ResizeEast"_s);
        break;

    case Type::NorthResize:
    case Type::NorthPanning:
        m_platformCursor = cursor("ResizeNorth"_s);
        break;

    case Type::NorthEastResize:
    case Type::NorthEastPanning:
        m_platformCursor = cursor("ResizeNortheast"_s);
        break;

    case Type::NorthWestResize:
    case Type::NorthWestPanning:
        m_platformCursor = cursor("ResizeNorthwest"_s);
        break;

    case Type::SouthResize:
    case Type::SouthPanning:
        m_platformCursor = cursor("ResizeSouth"_s);
        break;

    case Type::SouthEastResize:
    case Type::SouthEastPanning:
        m_platformCursor = cursor("ResizeSoutheast"_s);
        break;

    case Type::SouthWestResize:
    case Type::SouthWestPanning:
        m_platformCursor = cursor("ResizeSouthwest"_s);
        break;

    case Type::WestResize:
    case Type::WestPanning:
        m_platformCursor = cursor("ResizeWest"_s);
        break;

    case Type::NorthSouthResize:
        m_platformCursor = cursor("ResizeNorthSouth"_s);
        break;

    case Type::EastWestResize:
        m_platformCursor = cursor("ResizeEastWest"_s);
        break;

    case Type::NorthEastSouthWestResize:
        m_platformCursor = cursor("ResizeNortheastSouthwest"_s);
        break;

    case Type::NorthWestSouthEastResize:
        m_platformCursor = cursor("ResizeNorthwestSoutheast"_s);
        break;

    case Type::ColumnResize:
        m_platformCursor = [NSCursor resizeLeftRightCursor];
        break;

    case Type::RowResize:
        m_platformCursor = [NSCursor resizeUpDownCursor];
        break;

    case Type::VerticalText:
        m_platformCursor = [NSCursor IBeamCursorForVerticalLayout];
        break;

    case Type::Cell:
        m_platformCursor = cursor("Cell"_s);
        break;

    case Type::ContextMenu:
        m_platformCursor = [NSCursor contextualMenuCursor];
        break;

    case Type::Alias:
        m_platformCursor = cursor("MakeAlias"_s);
        break;

    case Type::Progress:
        m_platformCursor = cursor("BusyButClickable"_s);
        break;

    case Type::NoDrop:
        m_platformCursor = [NSCursor operationNotAllowedCursor];
        break;

    case Type::Copy:
        m_platformCursor = [NSCursor dragCopyCursor];
        break;

    case Type::None:
#if ENABLE(CUSTOM_CURSOR_SUPPORT)
        m_platformCursor = adoptNS([[NSCursor alloc] initWithImage:adoptNS([[NSImage alloc] initWithSize:NSMakeSize(1, 1)]).get() hotSpot:NSZeroPoint]);
#else
        m_platformCursor = [NSCursor arrowCursor];
#endif
        break;

    case Type::NotAllowed:
        m_platformCursor = [NSCursor operationNotAllowedCursor];
        break;

    case Type::ZoomIn:
        m_platformCursor = cursor("ZoomIn"_s);
        break;

    case Type::ZoomOut:
        m_platformCursor = cursor("ZoomOut"_s);
        break;

    case Type::Grab:
        m_platformCursor = [NSCursor openHandCursor];
        break;

    case Type::Grabbing:
        m_platformCursor = [NSCursor closedHandCursor];
        break;

    case Type::Custom:
#if ENABLE(CUSTOM_CURSOR_SUPPORT)
#if ENABLE(MOUSE_CURSOR_SCALE)
        m_platformCursor = createCustomCursor(m_image.get(), m_hotSpot, m_imageScaleFactor);
#else
        m_platformCursor = createCustomCursor(m_image.get(), m_hotSpot);
#endif // ENABLE(MOUSE_CURSOR_SCALE)
#endif // ENABLE(CUSTOM_CURSOR_SUPPORT)
        break;

    case Type::Invalid:
    default:
        ASSERT_NOT_REACHED();
    }
}

NSCursor *Cursor::platformCursor() const
{
    ensurePlatformCursor();
    return m_platformCursor.get();
}

void Cursor::setAsPlatformCursor() const
{
    RetainPtr cursor = platformCursor();
    if ([NSCursor currentCursor] == cursor)
        return;
    [cursor set];
}

} // namespace WebCore

#endif // HAVE(NSCURSOR)
