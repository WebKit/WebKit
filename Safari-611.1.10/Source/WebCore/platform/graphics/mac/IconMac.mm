/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#import "config.h"
#import "Icon.h"

#if PLATFORM(MAC)

#import "GraphicsContext.h"
#import "IntRect.h"
#import "LocalCurrentGraphicsContext.h"
#import "UTIUtilities.h"
#import <wtf/RefPtr.h>
#import <wtf/text/WTFString.h>

namespace WebCore {

Icon::Icon(NSImage *image)
    : m_nsImage(image)
{
    // Need this because WebCore uses AppKit's flipped coordinate system exclusively.
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [image setFlipped:YES];
    ALLOW_DEPRECATED_DECLARATIONS_END
}

Icon::~Icon()
{
}

// FIXME: Move the code to ChromeClient::iconForFiles().
RefPtr<Icon> Icon::createIconForFiles(const Vector<String>& filenames)
{
    if (filenames.isEmpty())
        return nullptr;

    bool useIconFromFirstFile;
    useIconFromFirstFile = filenames.size() == 1;
    if (useIconFromFirstFile) {
        // Don't pass relative filenames -- we don't want a result that depends on the current directory.
        // Need 0U here to disambiguate String::operator[] from operator(NSString*, int)[]
        if (filenames[0].isEmpty() || filenames[0][0U] != '/')
            return nullptr;

        NSImage *image = [[NSWorkspace sharedWorkspace] iconForFile:filenames[0]];
        if (!image)
            return nullptr;

        return adoptRef(new Icon(image));
    }
    NSImage *image = [NSImage imageNamed:NSImageNameMultipleDocuments];
    if (!image)
        return nullptr;

    return adoptRef(new Icon(image));
}

RefPtr<Icon> Icon::createIconForFileExtension(const String& fileExtension)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    NSImage *image = [[NSWorkspace sharedWorkspace] iconForFileType:[@"." stringByAppendingString:fileExtension]];
    ALLOW_DEPRECATED_DECLARATIONS_END
    if (!image)
        return nullptr;

    return adoptRef(new Icon(image));
}

RefPtr<Icon> Icon::createIconForUTI(const String& UTI)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    NSImage *image = [[NSWorkspace sharedWorkspace] iconForFileType:UTI];
    ALLOW_DEPRECATED_DECLARATIONS_END
    if (!image)
        return nullptr;

    return adoptRef(new Icon(image));
}

void Icon::paint(GraphicsContext& context, const FloatRect& rect)
{
    if (context.paintingDisabled())
        return;

    LocalCurrentGraphicsContext localCurrentGC(context);

    [m_nsImage drawInRect:rect fromRect:NSMakeRect(0, 0, [m_nsImage size].width, [m_nsImage size].height) operation:NSCompositingOperationSourceOver fraction:1.0f];
}

}

#endif // PLATFORM(MAC)
