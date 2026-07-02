/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
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
 * \brief Directory iterator.
 *//*--------------------------------------------------------------------*/

#include "deDirectoryIterator.hpp"
#include "deString.h"

#if (DE_DIRITER == DE_DIRITER_WIN32)
#include <direct.h> /* _chdir() */
#include <io.h>     /* _findfirst(), _findnext() */
#endif

namespace de
{

#if (DE_DIRITER == DE_DIRITER_WIN32)

DirectoryIterator::DirectoryIterator(const FilePath &path) : m_path(FilePath::normalize(path))
{
    DE_CHECK_RUNTIME_ERR(m_path.exists());
    DE_CHECK_RUNTIME_ERR(m_path.getType() == FilePath::TYPE_DIRECTORY);

    m_handle  = _findfirst32((std::string(m_path.getPath()) + "/*").c_str(), &m_fileInfo);
    m_hasItem = m_handle != -1;

    skipCurAndParent();
}

DirectoryIterator::~DirectoryIterator(void)
{
    if (m_handle != -1)
        _findclose(m_handle);
}

bool DirectoryIterator::hasItem(void) const
{
    return m_hasItem;
}

FilePath DirectoryIterator::getItem(void) const
{
    DE_ASSERT(hasItem());
    return FilePath::join(m_path, m_fileInfo.name);
}

void DirectoryIterator::next(void)
{
    m_hasItem = (_findnext32(m_handle, &m_fileInfo) == 0);
    skipCurAndParent();
}

void DirectoryIterator::skipCurAndParent(void)
{
    while (m_hasItem && (strcmp(m_fileInfo.name, "..") == 0 || strcmp(m_fileInfo.name, ".") == 0))
        m_hasItem = (_findnext32(m_handle, &m_fileInfo) == 0);
}

#elif (DE_DIRITER == DE_DIRITER_POSIX)

DirectoryIterator::DirectoryIterator(const FilePath &path)
    : m_path(FilePath::normalize(path))
    , m_handle(nullptr)
    , m_curEntry(nullptr)
{
    DE_CHECK_RUNTIME_ERR(m_path.exists());
    DE_CHECK_RUNTIME_ERR(m_path.getType() == FilePath::TYPE_DIRECTORY);

    m_handle = opendir(m_path.getPath());
    DE_CHECK_RUNTIME_ERR(m_handle);

    // Find first entry
    next();
}

DirectoryIterator::~DirectoryIterator(void)
{
    closedir(m_handle);
}

bool DirectoryIterator::hasItem(void) const
{
    return (m_curEntry != nullptr);
}

FilePath DirectoryIterator::getItem(void) const
{
    DE_ASSERT(hasItem());
    return FilePath::join(m_path, m_curEntry->d_name);
}

void DirectoryIterator::next(void)
{
    do
    {
        m_curEntry = readdir(m_handle);
    } while (m_curEntry && (strcmp(m_curEntry->d_name, "..") == 0 || strcmp(m_curEntry->d_name, ".") == 0));
}

#endif

} // namespace de
