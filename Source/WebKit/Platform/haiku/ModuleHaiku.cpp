/*
    Copyright (C) 2014 Haiku, Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "Module.h"

#include <wtf/text/CString.h>

namespace WebKit {

bool Module::load()
{
    m_module = load_add_on(m_path.utf8().data());
    if (!m_module) {
        return false;
    }

    return true;
}

void Module::unload()
{
    unload_add_on(m_module);
    m_module = 0;
}

void* Module::platformFunctionPointer(const char* functionName) const
{
    if (m_module) {
        void* pointer = NULL;
        get_image_symbol(m_module, functionName, B_SYMBOL_TYPE_ANY, &pointer);
        return pointer;
    }

    return 0;
}

}

