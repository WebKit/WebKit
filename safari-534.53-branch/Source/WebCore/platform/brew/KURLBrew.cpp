/*
 * Copyright (C) 2010 Company 100, Inc.
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
#include "KURL.h"

#include <wtf/text/CString.h>

#include <AEEFile.h>

namespace WebCore {

String KURL::fileSystemPath() const
{
    // Access files relative to the current module directory in a case-sensitive manner.
    // As IWeb "file:" engine opens files in BREW's application-relative file namespace,
    // we follow the same policy here.
    return String(AEEFS_HOME_DIR) + path();
}

}
