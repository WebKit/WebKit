/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2008, 2009-2010, 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 - 2010  Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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
 */

#pragma once

#include <wtf/text/WTFString.h>

// Helper functions for converting from CSSValues to text.

namespace WebCore {

// Common serializing methods. See: http://dev.w3.org/csswg/cssom/#common-serializing-idioms
void serializeIdentifier(const String& identifier, StringBuilder& appendTo, bool skipStartChecks = false);
void serializeString(const String&, StringBuilder& appendTo);
String serializeString(const String&);
String serializeURL(const String&);
String serializeFontFamily(const String&);

// FIXME-NEWPARSER: This hybrid "check for both string or ident" function can be removed
// once we have enabled CSSCustomIdentValue and CSSStringValue.
String serializeAsStringOrCustomIdent(const String&);

} // namespace WebCore
