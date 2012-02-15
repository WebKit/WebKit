/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "config.h"
#include "Icon.h"

#include "GraphicsContext.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "PlatformString.h"


namespace WebCore {

Icon::Icon()
{
}

Icon::~Icon()
{
}

// FIXME: Move the code to ChromeClient::iconForFiles().
PassRefPtr<Icon> Icon::createIconForFiles(const Vector<String>&)
{
    // FIXME: Should use QMimeType in Qt 5.
    notImplemented();
    return 0;
}

void Icon::paint(GraphicsContext*, const IntRect&)
{
    notImplemented();
}

}

// vim: ts=4 sw=4 et
