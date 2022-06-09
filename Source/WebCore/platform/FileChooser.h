/*
 * Copyright (C) 2006-2020 Apple Inc. All rights reserved.
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
 *
 */

#pragma once

#include <wtf/EnumTraits.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

enum MediaCaptureType {
    MediaCaptureTypeNone,
    MediaCaptureTypeUser,
    MediaCaptureTypeEnvironment
};

class FileChooser;
class Icon;

struct FileChooserFileInfo {
    FileChooserFileInfo isolatedCopy() const
    {
        return { path.isolatedCopy(), replacementPath.isolatedCopy(), displayName.isolatedCopy() };
    }

    const String path;
    const String replacementPath;
    const String displayName;
};

struct FileChooserSettings {
    bool allowsDirectories { false };
    bool allowsMultipleFiles { false };
    Vector<String> acceptMIMETypes;
    Vector<String> acceptFileExtensions;
    Vector<String> selectedFiles;
#if ENABLE(MEDIA_CAPTURE)
    MediaCaptureType mediaCaptureType { MediaCaptureTypeNone };
#endif
};

class FileChooserClient {
public:
    virtual ~FileChooserClient() = default;

    virtual void filesChosen(const Vector<FileChooserFileInfo>&, const String& displayString = { }, Icon* = nullptr) = 0;
};

class FileChooser : public RefCounted<FileChooser> {
public:
    static Ref<FileChooser> create(FileChooserClient*, const FileChooserSettings&);
    WEBCORE_EXPORT ~FileChooser();

    void invalidate();

    WEBCORE_EXPORT void chooseFile(const String& path);
    WEBCORE_EXPORT void chooseFiles(const Vector<String>& paths, const Vector<String>& replacementPaths = { });
#if PLATFORM(IOS_FAMILY)
    // FIXME: This function is almost identical to FileChooser::chooseFiles(). We should merge this
    // function with FileChooser::chooseFiles() and hence remove the PLATFORM(IOS_FAMILY)-guard.
    WEBCORE_EXPORT void chooseMediaFiles(const Vector<String>& paths, const String& displayString, Icon*);
#endif

    // FIXME: We should probably just pass file paths that could be virtual paths with proper display names rather than passing structs.
    void chooseFiles(const Vector<FileChooserFileInfo>& files);

    const FileChooserSettings& settings() const { return m_settings; }

private:
    FileChooser(FileChooserClient*, const FileChooserSettings&);

    FileChooserClient* m_client { nullptr };
    FileChooserSettings m_settings;
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::MediaCaptureType> {
    using values = EnumValues<
        WebCore::MediaCaptureType,
        WebCore::MediaCaptureType::MediaCaptureTypeNone,
        WebCore::MediaCaptureType::MediaCaptureTypeUser,
        WebCore::MediaCaptureType::MediaCaptureTypeEnvironment
    >;
};

} // namespace WTF
