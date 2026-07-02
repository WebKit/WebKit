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

#include "deFilePath.hpp"

#include <vector>
#include <stdexcept>

#include <sys/stat.h>
#include <sys/types.h>

#if (DE_OS == DE_OS_WIN32)
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

using std::string;

namespace de
{

#if (DE_OS == DE_OS_WIN32)
const std::string FilePath::separator = "\\";
#else
const std::string FilePath::separator = "/";
#endif

FilePath::FilePath(const std::vector<std::string> &components)
{
    for (size_t ndx = 0; ndx < components.size(); ndx++)
    {
        if (!m_path.empty() && !isSeparator(m_path[m_path.size() - 1]))
            m_path += separator;
        m_path += components[ndx];
    }
}

void FilePath::split(std::vector<std::string> &components) const
{
    components.clear();

    int curCompStart = 0;
    int pos;

    if (isWinNetPath())
        components.push_back(separator + separator);
    else if (isRootPath() && !beginsWithDrive())
        components.push_back(separator);

    for (pos = 0; pos < (int)m_path.length(); pos++)
    {
        const char c = m_path[pos];

        if (isSeparator(c))
        {
            if (pos - curCompStart > 0)
                components.push_back(m_path.substr(curCompStart, pos - curCompStart));

            curCompStart = pos + 1;
        }
    }

    if (pos - curCompStart > 0)
        components.push_back(m_path.substr(curCompStart, pos - curCompStart));
}

FilePath FilePath::join(const std::vector<std::string> &components)
{
    return FilePath(components);
}

FilePath &FilePath::normalize(void)
{
    std::vector<std::string> components;
    std::vector<std::string> reverseNormalizedComponents;

    split(components);

    m_path = "";

    int numUp = 0;

    // Do in reverse order and eliminate any . or .. components
    for (int ndx = (int)components.size() - 1; ndx >= 0; ndx--)
    {
        const std::string &comp = components[ndx];
        if (comp == "..")
            numUp += 1;
        else if (comp == ".")
            continue;
        else if (numUp > 0)
            numUp -= 1; // Skip part
        else
            reverseNormalizedComponents.push_back(comp);
    }

    if (isAbsolutePath() && numUp > 0)
        throw std::runtime_error("Cannot normalize path: invalid path");

    // Prepend necessary ".." components
    while (numUp--)
        reverseNormalizedComponents.push_back("..");

    if (reverseNormalizedComponents.empty() && components.back() == ".")
        reverseNormalizedComponents.push_back("."); // Composed of "." components only

    *this = join(std::vector<std::string>(reverseNormalizedComponents.rbegin(), reverseNormalizedComponents.rend()));

    return *this;
}

FilePath FilePath::normalize(const FilePath &path)
{
    return FilePath(path).normalize();
}

std::string FilePath::getBaseName(void) const
{
    std::vector<std::string> components;
    split(components);
    return !components.empty() ? components[components.size() - 1] : std::string("");
}

std::string FilePath::getDirName(void) const
{
    std::vector<std::string> components;
    split(components);
    if (components.size() > 1)
    {
        components.pop_back();
        return FilePath(components).getPath();
    }
    else if (isAbsolutePath())
        return separator;
    else
        return std::string(".");
}

std::string FilePath::getFileExtension(void) const
{
    std::string baseName = getBaseName();
    size_t dotPos        = baseName.find_last_of('.');
    if (dotPos == std::string::npos)
        return std::string("");
    else
        return baseName.substr(dotPos + 1);
}

bool FilePath::exists(void) const
{
    FilePath normPath = FilePath::normalize(*this);
    struct stat st;
    int result = stat(normPath.getPath(), &st);
    return result == 0;
}

FilePath::Type FilePath::getType(void) const
{
    FilePath normPath = FilePath::normalize(*this);
    struct stat st;
    int result = stat(normPath.getPath(), &st);

    if (result != 0)
        return TYPE_UNKNOWN;

    int type = st.st_mode & S_IFMT;
    if (type == S_IFREG)
        return TYPE_FILE;
    else if (type == S_IFDIR)
        return TYPE_DIRECTORY;
    else
        return TYPE_UNKNOWN;
}

bool FilePath::beginsWithDrive(void) const
{
    for (int ndx = 0; ndx < (int)m_path.length(); ndx++)
    {
        if (m_path[ndx] == ':' && ndx + 1 < (int)m_path.length() && isSeparator(m_path[ndx + 1]))
            return true; // First part is drive letter.
        if (isSeparator(m_path[ndx]))
            return false;
    }
    return false;
}

bool FilePath::isAbsolutePath(void) const
{
    return isRootPath() || isWinNetPath() || beginsWithDrive();
}

void FilePath_selfTest(void)
{
    DE_TEST_ASSERT(!FilePath(".").isAbsolutePath());
    DE_TEST_ASSERT(!FilePath("..\\foo").isAbsolutePath());
    DE_TEST_ASSERT(!FilePath("foo").isAbsolutePath());
    DE_TEST_ASSERT(FilePath("\\foo/bar").isAbsolutePath());
    DE_TEST_ASSERT(FilePath("/foo").isAbsolutePath());
    DE_TEST_ASSERT(FilePath("\\").isAbsolutePath());
    DE_TEST_ASSERT(FilePath("\\\\net\\loc").isAbsolutePath());
    DE_TEST_ASSERT(FilePath("C:\\file.txt").isAbsolutePath());
    DE_TEST_ASSERT(FilePath("c:/file.txt").isAbsolutePath());

    DE_TEST_ASSERT(string(".") == FilePath(".//.").normalize().getPath());
    DE_TEST_ASSERT(string(".") == FilePath(".").normalize().getPath());
    DE_TEST_ASSERT((string("..") + FilePath::separator + "test") ==
                   FilePath("foo/../bar/../../test").normalize().getPath());
    DE_TEST_ASSERT((FilePath::separator + "foo" + FilePath::separator + "foo.txt") ==
                   FilePath("/foo\\bar/..\\dir\\..\\foo.txt").normalize().getPath());
    DE_TEST_ASSERT((string("c:") + FilePath::separator + "foo" + FilePath::separator + "foo.txt") ==
                   FilePath("c:/foo\\bar/..\\dir\\..\\foo.txt").normalize().getPath());
    DE_TEST_ASSERT((FilePath::separator + FilePath::separator + "foo" + FilePath::separator + "foo.txt") ==
                   FilePath("\\\\foo\\bar/..\\dir\\..\\foo.txt").normalize().getPath());

    DE_TEST_ASSERT(FilePath("foo/bar").getBaseName() == "bar");
    DE_TEST_ASSERT(FilePath("foo/bar/").getBaseName() == "bar");
    DE_TEST_ASSERT(FilePath("foo\\bar").getBaseName() == "bar");
    DE_TEST_ASSERT(FilePath("foo\\bar\\").getBaseName() == "bar");
    DE_TEST_ASSERT(FilePath("foo/bar").getDirName() == "foo");
    DE_TEST_ASSERT(FilePath("foo/bar/").getDirName() == "foo");
    DE_TEST_ASSERT(FilePath("foo\\bar").getDirName() == "foo");
    DE_TEST_ASSERT(FilePath("foo\\bar\\").getDirName() == "foo");
    DE_TEST_ASSERT(FilePath("/foo/bar/baz").getDirName() == FilePath::separator + "foo" + FilePath::separator + "bar");
}

static void createDirectoryImpl(const char *path)
{
#if (DE_OS == DE_OS_WIN32)
    if (!CreateDirectory(path, nullptr))
        throw std::runtime_error("Failed to create directory");
#elif (DE_OS == DE_OS_UNIX) || (DE_OS == DE_OS_OSX) || (DE_OS == DE_OS_IOS) || (DE_OS == DE_OS_ANDROID) || \
    (DE_OS == DE_OS_SYMBIAN) || (DE_OS == DE_OS_QNX) || (DE_OS == DE_OS_FUCHSIA)
    if (mkdir(path, 0777) != 0)
        throw std::runtime_error("Failed to create directory");
#else
#error Implement createDirectoryImpl() for your platform.
#endif
}

void createDirectory(const char *path)
{
    FilePath dirPath = FilePath::normalize(path);
    FilePath parentPath(dirPath.getDirName());

    if (dirPath.exists())
        throw std::runtime_error("Destination exists already");
    else if (!parentPath.exists())
        throw std::runtime_error("Parent directory doesn't exist");
    else if (parentPath.getType() != FilePath::TYPE_DIRECTORY)
        throw std::runtime_error("Parent is not directory");

    createDirectoryImpl(path);
}

void createDirectoryAndParents(const char *path)
{
    std::vector<std::string> createPaths;
    FilePath curPath(path);

    if (curPath.exists())
        throw std::runtime_error("Destination exists already");

    while (!curPath.exists())
    {
        createPaths.push_back(curPath.getPath());

        std::string parent = curPath.getDirName();
        DE_CHECK_RUNTIME_ERR(parent != curPath.getPath());
        curPath = FilePath(parent);
    }

    // Create in reverse order.
    for (std::vector<std::string>::const_reverse_iterator parentIter = createPaths.rbegin();
         parentIter != createPaths.rend(); parentIter++)
        createDirectory(parentIter->c_str());
}

} // namespace de
