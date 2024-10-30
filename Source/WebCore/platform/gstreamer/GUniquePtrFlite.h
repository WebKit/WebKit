/*
 *  Copyright (C) 2023 Igalia S.L
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
 */

#pragma once

#include <wtf/Platform.h>

#if USE(FLITE)

#include <flite/flite.h>
#include <wtf/glib/GUniquePtr.h>

namespace WTF {

WTF_DEFINE_GPTR_DELETER(cst_voice, delete_voice)
WTF_DEFINE_GPTR_DELETER(cst_wave, delete_wave)

} // namespace WTF

#endif // USE(FLITE)
