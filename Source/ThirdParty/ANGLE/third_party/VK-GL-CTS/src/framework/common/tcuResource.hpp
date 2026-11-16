#ifndef _TCURESOURCE_HPP
#define _TCURESOURCE_HPP
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
 * \brief Resource system.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"

#include <string>

// \todo [2010-07-31 pyry] Move Archive and File* to separate files

namespace tcu
{

/*--------------------------------------------------------------------*//*!
 * \brief Resource object
 *
 * Test framework uses abstraction for data access instead of using
 * files directly to better support platforms where application resources
 * are not available directly in the filesystem.
 *
 * Resource objects are requested from Archive object provided by Platform.
 * The user is responsible of disposing the objects afterwards.
 *//*--------------------------------------------------------------------*/
class Resource
{
public:
    virtual ~Resource(void)
    {
    }

    virtual void read(uint8_t *dst, int numBytes) = 0;
    virtual uint32_t getSize(void) const          = 0;
    virtual uint32_t getPosition(void) const      = 0;
    virtual void setPosition(uint32_t position)   = 0;

    const std::string &getName(void) const
    {
        return m_name;
    }

protected:
    Resource(const std::string &name) : m_name(name)
    {
    }

private:
    std::string m_name;
};

/*--------------------------------------------------------------------*//*!
 * \brief Abstract resource archive
 *//*--------------------------------------------------------------------*/
class Archive
{
public:
    virtual ~Archive(void)
    {
    }

    /*--------------------------------------------------------------------*//*!
     * \brief Open resource
     *
     * Throws resource error if no resource with given name exists.
     *
     * Resource object must be deleted after use
     *
     * \param name Resource path
     * \return Resource object
     *//*--------------------------------------------------------------------*/
    virtual Resource *getResource(const char *name) const = 0;

protected:
    Archive()
    {
    }
};

/*--------------------------------------------------------------------*//*!
 * \brief Directory-based archive implementation
 *//*--------------------------------------------------------------------*/
class DirArchive : public Archive
{
public:
    DirArchive(const char *path);
    ~DirArchive(void);

    Resource *getResource(const char *name) const;

    // \note Assignment and copy allowed
    DirArchive(const DirArchive &other) : Archive(), m_path(other.m_path)
    {
    }
    DirArchive &operator=(const DirArchive &other)
    {
        m_path = other.m_path;
        return *this;
    }

private:
    std::string m_path;
};

class FileResource : public Resource
{
public:
    FileResource(const char *filename);
    ~FileResource(void);

    void read(uint8_t *dst, int numBytes);
    void read(uint16_t *dst, int numBytes);
    uint32_t getSize(void) const;
    uint32_t getPosition(void) const;
    void setPosition(uint32_t position);

private:
    FileResource(const FileResource &other);
    FileResource &operator=(const FileResource &other);

    FILE *m_file;
};

class ResourcePrefix : public Archive
{
public:
    ResourcePrefix(const Archive &archive, const char *prefix);
    virtual ~ResourcePrefix(void)
    {
    }

    virtual Resource *getResource(const char *name) const;

private:
    const Archive &m_archive;
    std::string m_prefix;
};

} // namespace tcu

#endif // _TCURESOURCE_HPP
