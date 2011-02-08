/*
 *  Copyright (C) 2011 Collabora Ltd.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

// Use this file to assert that various WebKit API enum values continue
// matching WebCore defined enum values.

#include "config.h"

#include "DumpRenderTreeSupportGtk.h"
#include "FindOptions.h"
#include <wtf/Assertions.h>

#define COMPILE_ASSERT_MATCHING_ENUM(webkit_name, webcore_name) \
    COMPILE_ASSERT(int(WebKit::webkit_name) == int(WebCore::webcore_name), mismatching_enums)

COMPILE_ASSERT_MATCHING_ENUM(WebFindOptionsCaseInsensitive , CaseInsensitive);
COMPILE_ASSERT_MATCHING_ENUM(WebFindOptionsAtWordStarts, AtWordStarts);
COMPILE_ASSERT_MATCHING_ENUM(WebFindOptionsTreatMedialCapitalAsWordStart, TreatMedialCapitalAsWordStart);
COMPILE_ASSERT_MATCHING_ENUM(WebFindOptionsBackwards, Backwards);
COMPILE_ASSERT_MATCHING_ENUM(WebFindOptionsWrapAround, WrapAround);
COMPILE_ASSERT_MATCHING_ENUM(WebFindOptionsStartInSelection, StartInSelection);
