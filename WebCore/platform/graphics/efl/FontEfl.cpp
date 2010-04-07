/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006 Apple Inc. All rights reserved.
 * Copyright (C) 2008 INdT - Instituto Nokia de Tecnologia
 * Copyright (C) 2009-2010 ProFUSION embedded systems
 * Copyright (C) 2009-2010 Samsung Electronics
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
#include "Font.h"

#include "GraphicsContext.h"
#include "NotImplemented.h"
#include "SimpleFontData.h"

#include <cairo.h>

namespace WebCore {

void Font::drawComplexText(GraphicsContext*, const TextRun&, const FloatPoint&, int from, int to) const
{
    notImplemented();
}

bool Font::canReturnFallbackFontsForComplexText()
{
    return false;
}

float Font::floatWidthForComplexText(const TextRun&, HashSet<const SimpleFontData*>*, GlyphOverflow*) const
{
    notImplemented();
    return 0.0f;
}

int Font::offsetForPositionForComplexText(const TextRun&, int, bool) const
{
    notImplemented();
    return 0;
}

FloatRect Font::selectionRectForComplexText(const TextRun&, const IntPoint&, int, int, int) const
{
    notImplemented();
    return FloatRect();
}

}
