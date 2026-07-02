#ifndef _DEFILEPATH_HPP
#define _DEFILEPATH_HPP
/*-------------------------------------------------------------------------
 * drawElements C++ Base Library
 * -----------------------------
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
 * \brief Filesystem path class.
 *//*--------------------------------------------------------------------*/

#include "deDefs.hpp"

#include <string>
#include <vector>

namespace de
{

void FilePath_selfTest(void);

class FilePath
{
public:
    enum Type
    {
        TYPE_UNKNOWN = 0, /*!< Non-existent or unknown object.    */
        TYPE_FILE,        /*!< File.                                */
        TYPE_DIRECTORY,   /*!< Directory.                            */

        TYPE_LAST
    };

    static const std::string separator; /*!< Path separator.        */

    FilePath(void);
    FilePath(const std::string &path);
    FilePath(const char *path);
    FilePath(const std::vector<std::string> &components);
    ~FilePath(void);

    bool exists(void) const;
    Type getType(void) const;

    const char *getPath(void) const;
    std::string getBaseName(void) const;
    std::string getDirName(void) const;
    std::string getFileExtension(void) const;

    static FilePath join(const FilePath &a, const FilePath &b);
    FilePath &join(const FilePath &b);

    static FilePath normalize(const FilePath &path);
    FilePath &normalize(void);

    void split(std::vector<std::string> &components) const;
    static FilePath join(const std::vector<std::string> &components);

    bool isAbsolutePath(void) const;

    static bool isSeparator(char c);

private:
    bool isRootPath(void) const;
    bool isWinNetPath(void) const;
    bool beginsWithDrive(void) const;

    std::string m_path;
};

// \todo [2012-09-05 pyry] Move to delibs?
void createDirectory(const char *path);
void createDirectoryAndParents(const char *path);

inline FilePath::FilePath(void)
{
}

inline FilePath::FilePath(const std::string &path) : m_path(path)
{
}

inline FilePath::FilePath(const char *path) : m_path(path)
{
}

inline FilePath::~FilePath()
{
}

inline FilePath &FilePath::join(const FilePath &b)
{
    if (m_path == "")
        m_path = b.m_path;
    else
        m_path += separator + b.m_path;
    return *this;
}

inline FilePath FilePath::join(const FilePath &a, const FilePath &b)
{
    return FilePath(a).join(b);
}

inline const char *FilePath::getPath(void) const
{
    return m_path.c_str();
}

inline bool FilePath::isSeparator(char c)
{
    return c == '/' || c == '\\';
}

inline bool FilePath::isRootPath(void) const
{
    return m_path.length() >= 1 && isSeparator(m_path[0]);
}

inline bool FilePath::isWinNetPath(void) const
{
    return m_path.length() >= 2 && isSeparator(m_path[0]) && isSeparator(m_path[1]);
}

} // namespace de

#endif // _DEFILEPATH_HPP
