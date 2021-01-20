/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "File.h"

#include "BlobURL.h"
#include "MIMETypeRegistry.h"
#include "ThreadableBlobRegistry.h"
#include <wtf/DateMath.h>
#include <wtf/FileMetadata.h>
#include <wtf/FileSystem.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(File);

Ref<File> File::createWithRelativePath(ScriptExecutionContext* context, const String& path, const String& relativePath)
{
    auto file = File::create(context, path);
    file->setRelativePath(relativePath);
    return file;
}

Ref<File> File::create(ScriptExecutionContext* context, const String& path, const String& replacementPath, const String& nameOverride)
{
    String name;
    String type;
    String effectivePath = !replacementPath.isNull() ? replacementPath : path;
    computeNameAndContentType(effectivePath, nameOverride, name, type);

    auto internalURL = BlobURL::createInternalURL();
    ThreadableBlobRegistry::registerFileBlobURL(internalURL, path, replacementPath, type);

    auto file = adoptRef(*new File(context, WTFMove(internalURL), WTFMove(type), WTFMove(effectivePath), WTFMove(name)));
    file->suspendIfNeeded();
    return file;
}

File::File(ScriptExecutionContext* context, URL&& url, String&& type, String&& path, String&& name)
    : Blob(uninitializedContructor, context, WTFMove(url), WTFMove(type))
    , m_path(WTFMove(path))
    , m_name(WTFMove(name))
{
}

File::File(DeserializationContructor, ScriptExecutionContext* context, const String& path, const URL& url, const String& type, const String& name, const Optional<int64_t>& lastModified)
    : Blob(deserializationContructor, context, url, type, { }, path)
    , m_path(path)
    , m_name(name)
    , m_lastModifiedDateOverride(lastModified)
{
}

File::File(ScriptExecutionContext& context, Vector<BlobPartVariant>&& blobPartVariants, const String& filename, const PropertyBag& propertyBag)
    : Blob(context, WTFMove(blobPartVariants), propertyBag)
    , m_name(filename)
    , m_lastModifiedDateOverride(propertyBag.lastModified.valueOr(WallTime::now().secondsSinceEpoch().milliseconds()))
{
}

File::File(ScriptExecutionContext* context, const Blob& blob, const String& name)
    : Blob(referencingExistingBlobConstructor, context, blob)
    , m_name(name)
{
    ASSERT(!blob.isFile());
}

File::File(ScriptExecutionContext* context, const File& file, const String& name)
    : Blob(referencingExistingBlobConstructor, context, file)
    , m_path(file.path())
    , m_relativePath(file.relativePath())
    , m_name(!name.isNull() ? name : file.name())
    , m_lastModifiedDateOverride(file.m_lastModifiedDateOverride)
    , m_isDirectory(file.isDirectory())
{
}

int64_t File::lastModified() const
{
    if (m_lastModifiedDateOverride)
        return m_lastModifiedDateOverride.value();

    int64_t result;

    // FIXME: This does sync-i/o on the main thread and also recalculates every time the method is called.
    // The i/o should be performed on a background thread,
    // and the result should be cached along with an asynchronous monitor for changes to the file.
    auto modificationTime = FileSystem::getFileModificationTime(m_path);
    if (modificationTime)
        result = modificationTime->secondsSinceEpoch().millisecondsAs<int64_t>();
    else
        result = WallTime::now().secondsSinceEpoch().millisecondsAs<int64_t>();

    return WTF::timeClip(result);
}

void File::computeNameAndContentType(const String& path, const String& nameOverride, String& effectiveName, String& effectiveContentType)
{
#if ENABLE(FILE_REPLACEMENT)
    if (shouldReplaceFile(path)) {
        computeNameAndContentTypeForReplacedFile(path, nameOverride, effectiveName, effectiveContentType);
        return;
    }
#endif
    effectiveName = nameOverride.isEmpty() ? FileSystem::pathGetFileName(path) : nameOverride;
    size_t index = effectiveName.reverseFind('.');
    if (index != notFound) {
        callOnMainThreadAndWait([&effectiveContentType, &effectiveName, index] {
            effectiveContentType = MIMETypeRegistry::mimeTypeForExtension(effectiveName.substring(index + 1)).isolatedCopy();
        });
    }
}

String File::contentTypeForFile(const String& path)
{
    String name;
    String type;
    computeNameAndContentType(path, String(), name, type);

    return type;
}

bool File::isDirectory() const
{
    if (!m_isDirectory)
        m_isDirectory = FileSystem::fileIsDirectory(m_path, FileSystem::ShouldFollowSymbolicLinks::Yes);
    return *m_isDirectory;
}

const char* File::activeDOMObjectName() const
{
    return "File";
}

} // namespace WebCore
