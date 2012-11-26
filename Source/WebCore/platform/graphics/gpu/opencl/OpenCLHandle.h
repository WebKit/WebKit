/*
 * Copyright (C) 2012 Tamas Czene <tczene@inf.u-szeged.hu>
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
#if ENABLE(OPENCL)

#ifndef OpenCLHandle_h
#define OpenCLHandle_h

#include "CL/cl.h"

namespace WebCore {

class OpenCLHandle {
public:
    OpenCLHandle() : m_openCLMemory(0) { }
    OpenCLHandle(cl_mem openCLMemory) : m_openCLMemory(openCLMemory) { }

    operator cl_mem() { return m_openCLMemory; }

    void operator=(OpenCLHandle openCLMemory) { m_openCLMemory = openCLMemory; }

    // This conversion operator allows implicit conversion to bool but not to other integer types.
    typedef cl_mem (OpenCLHandle::*UnspecifiedBoolType);
    operator UnspecifiedBoolType() const { return m_openCLMemory ? &OpenCLHandle::m_openCLMemory : 0; }

    void* handleAddress() { return reinterpret_cast<void*>(&m_openCLMemory); }

    void clear()
    {
        if (m_openCLMemory)
            clReleaseMemObject(m_openCLMemory);
        m_openCLMemory = 0;
    }

private:
    cl_mem m_openCLMemory;
};

}

#endif
#endif // ENABLE(OPENCL)
