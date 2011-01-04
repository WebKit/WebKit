/*
 *  Copyright (C) 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2008 Collabora Ltd.
 *  Copyright (C) 2009 Martin Robinson
 *  Copyright (C) 2010 Company 100, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef RefPtrBrew_h
#define RefPtrBrew_h

#include "RefPtr.h"
#include <AEEIBase.h>

namespace WTF {

// All Brew MP classes are derived from either IBase or IQI.
template<> void refIfNotNull(IBase* ptr)
{
    if (LIKELY(ptr != 0))
        IBase_AddRef(ptr);
}

template<> void derefIfNotNull(IBase* ptr)
{
    if (LIKELY(ptr != 0))
        IBase_Release(ptr);
}

template<> void refIfNotNull(IQI* ptr)
{
    if (LIKELY(ptr != 0))
        IQI_AddRef(ptr);
}

template<> void derefIfNotNull(IQI* ptr)
{
    if (LIKELY(ptr != 0))
        IQI_Release(ptr);
}

} // namespace WTF

#endif // RefPtrBrew_h
