#ifndef _DEDIRECTORYITERATOR_HPP
#define _DEDIRECTORYITERATOR_HPP
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

#include "deDefs.hpp"
#include "deFilePath.hpp"

#define DE_DIRITER_WIN32 0
#define DE_DIRITER_POSIX 1

#if (DE_OS == DE_OS_WIN32 && DE_COMPILER == DE_COMPILER_MSC)
#define DE_DIRITER DE_DIRITER_WIN32
#elif (DE_OS == DE_OS_UNIX) || (DE_OS == DE_OS_OSX) || (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_SYMBIAN) || \
    (DE_OS == DE_OS_IOS) || (DE_OS == DE_OS_QNX) ||                                                            \
    (DE_OS == DE_OS_WIN32 && (DE_COMPILER == DE_COMPILER_CLANG || DE_COMPILER == DE_COMPILER_GCC)) ||          \
    (DE_OS == DE_OS_FUCHSIA)
#define DE_DIRITER DE_DIRITER_POSIX
#endif

#if !defined(DE_DIRITER)
#error Implement tcu::DirectoryIterator for your platform.
#endif

#if (DE_DIRITER == DE_DIRITER_WIN32)
#include <io.h> /* _finddata32_t */
#elif (DE_DIRITER == DE_DIRITER_POSIX)
#include <sys/types.h>
#include <dirent.h>
#endif

namespace de
{

class DirectoryIterator
{
public:
    DirectoryIterator(const FilePath &path);
    ~DirectoryIterator(void);

    FilePath getItem(void) const;
    bool hasItem(void) const;
    void next(void);

private:
    DirectoryIterator(const DirectoryIterator &other);
    DirectoryIterator &operator=(const DirectoryIterator &other);

    FilePath m_path;

#if (DE_DIRITER == DE_DIRITER_WIN32)
    void skipCurAndParent(void);

    bool m_hasItem;
    intptr_t m_handle;
    struct _finddata32_t m_fileInfo;

#elif (DE_DIRITER == DE_DIRITER_POSIX)
    DIR *m_handle;
    struct dirent *m_curEntry;
#endif
};

} // namespace de

#endif // _DEDIRECTORYITERATOR_HPP
