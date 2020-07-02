/*
 * Copyright (C) 2007-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FileChooser.h"

namespace WebCore {

FileChooser::FileChooser(FileChooserClient* client, const FileChooserSettings& settings)
    : m_client(client)
    , m_settings(settings)
{
}

Ref<FileChooser> FileChooser::create(FileChooserClient* client, const FileChooserSettings& settings)
{
    return adoptRef(*new FileChooser(client, settings));
}

FileChooser::~FileChooser() = default;

void FileChooser::invalidate()
{
    ASSERT(m_client);

    m_client = nullptr;
}

void FileChooser::chooseFile(const String& filename)
{
    Vector<String> filenames;
    filenames.append(filename);
    chooseFiles(filenames);
}

void FileChooser::chooseFiles(const Vector<String>& filenames, const Vector<String>& replacementNames)
{
    // FIXME: This is inelegant. We should not be looking at settings here.
    if (m_settings.selectedFiles == filenames)
        return;

    if (!m_client)
        return;

    Vector<FileChooserFileInfo> files;
    for (size_t i = 0, size = filenames.size(); i < size; ++i)
        files.append({ filenames[i], i < replacementNames.size() ? replacementNames[i] : nullString(), { } });
    m_client->filesChosen(files);
}

#if PLATFORM(IOS_FAMILY)

// FIXME: This function is almost identical to FileChooser::chooseFiles(). We should merge this function
// with FileChooser::chooseFiles() and hence remove the PLATFORM(IOS_FAMILY)-guard.
void FileChooser::chooseMediaFiles(const Vector<String>& filenames, const String& displayString, Icon* icon)
{
    // FIXME: This is inelegant. We should not be looking at settings here.
    if (m_settings.selectedFiles == filenames)
        return;

    if (!m_client)
        return;

    Vector<FileChooserFileInfo> files;
    for (auto& filename : filenames)
        files.append({ filename, { }, { } });
    m_client->filesChosen(files, displayString, icon);
}

#endif

void FileChooser::chooseFiles(const Vector<FileChooserFileInfo>& files)
{
    Vector<String> paths;
    for (auto& file : files)
        paths.append(file.path);

    // FIXME: This is inelegant. We should not be looking at settings here.
    if (m_settings.selectedFiles == paths)
        return;

    if (m_client)
        m_client->filesChosen(files);
}

} // namespace WebCore
