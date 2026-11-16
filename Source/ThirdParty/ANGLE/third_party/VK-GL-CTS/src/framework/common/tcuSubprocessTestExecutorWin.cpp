/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 The Khronos Group Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
* \file
* \brief Subprocess test executor shared memory implementation file.
*//*--------------------------------------------------------------------*/

#include "tcuSubprocessTestExecutor.hpp"
#include <windows.h>

namespace tcu
{

std::pair<DWORD, DWORD> makeDWords(size_t size)
{
    const size_t shift  = sizeof(DWORD) * std::numeric_limits<uint8_t>::digits;
    const size_t hiSize = (sizeof(size_t) > sizeof(DWORD)) ? (size >> shift) : 0;
    return {DWORD(hiSize), DWORD(size)};
}

bool SubprocessTestExecutor::SharedMemory::allocate(size_t size, unsigned long &error, bool raise)
{
    DE_ASSERT(nullptr == m_handle && nullptr == m_data);
    const auto dWords = makeDWords(size);
    m_handle =
        CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, dWords.first, dWords.second, name.c_str());
    if (nullptr == m_handle)
    {
        error = GetLastError();
        if (raise)
            TCU_THROW(NotSupportedError, "CreateFileMapping failed, error=" + std::to_string(error));
        return false;
    }

    m_data = (SharedCase *)MapViewOfFile(m_handle, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (nullptr == m_data)
    {
        error = GetLastError();
        CloseHandle(m_handle);
        if (raise)
            TCU_THROW(NotSupportedError, "MapViewOfFile failed, error=" + std::to_string(error));
        return false;
    }

    m_state = true;
    m_size  = size;
    return true;
}

bool SubprocessTestExecutor::SharedMemory::open(size_t size, unsigned long &error, bool raise)
{
    DE_ASSERT(nullptr == m_handle && nullptr == m_data);
    m_handle = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, name.c_str());
    if (nullptr == m_handle)
    {
        error = GetLastError();
        if (raise)
            TCU_THROW(NotSupportedError, "OpenFileMapping failed, error=" + std::to_string(error));
        return false;
    }

    m_data = (SharedCase *)MapViewOfFile(m_handle, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (nullptr == m_data)
    {
        error = GetLastError();
        CloseHandle(m_handle);
        if (raise)
            TCU_THROW(NotSupportedError, "MapViewOfFile failed, error=" + std::to_string(error));
        return false;
    }

    m_state = false;
    m_size  = size;
    return true;
}

void SubprocessTestExecutor::SharedMemory::close()
{
    if (m_data)
    {
        UnmapViewOfFile(m_data);
        m_data = nullptr;
    }
    if (m_handle)
    {
        CloseHandle(m_handle);
        m_handle = nullptr;
    }
    m_state = {};
    m_size  = 0;
}

} // namespace tcu
