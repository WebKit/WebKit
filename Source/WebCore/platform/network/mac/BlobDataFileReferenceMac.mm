/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "BlobDataFileReference.h"

#if ENABLE(FILE_REPLACEMENT)

#include <wtf/FileMetadata.h>
#include <wtf/FileSystem.h>
#include <wtf/SoftLinking.h>
#include <wtf/text/CString.h>

#if USE(APPLE_INTERNAL_SDK)
#include <Bom/BOMCopier.h>
#endif

typedef struct _BOMCopier* BOMCopier;

SOFT_LINK_PRIVATE_FRAMEWORK(Bom)
SOFT_LINK(Bom, BOMCopierNew, BOMCopier, (), ())
SOFT_LINK(Bom, BOMCopierFree, void, (BOMCopier copier), (copier))
SOFT_LINK(Bom, BOMCopierCopyWithOptions, int, (BOMCopier copier, const char* fromObj, const char* toObj, CFDictionaryRef options), (copier, fromObj, toObj, options))

#define kBOMCopierOptionCreatePKZipKey CFSTR("createPKZip")
#define kBOMCopierOptionSequesterResourcesKey CFSTR("sequesterResources")
#define kBOMCopierOptionKeepParentKey CFSTR("keepParent")
#define kBOMCopierOptionCopyResourcesKey CFSTR("copyResources")

namespace WebCore {

void BlobDataFileReference::generateReplacementFile()
{
    ASSERT(m_replacementPath.isNull());
    ASSERT(m_replacementShouldBeGenerated);

    prepareForFileAccess();

    RetainPtr<NSFileCoordinator> coordinator = adoptNS([[NSFileCoordinator alloc] initWithFilePresenter:nil]);
    [coordinator coordinateReadingItemAtURL:[NSURL fileURLWithPath:m_path] options:NSFileCoordinatorReadingWithoutChanges error:nullptr byAccessor:^(NSURL *newURL) {
        // The archive is put into a subdirectory of temporary directory for historic reasons. Changing this will require WebCore to change at the same time.
        CString archivePath([NSTemporaryDirectory() stringByAppendingPathComponent:@"WebKitGeneratedFileXXXXXX"].fileSystemRepresentation);
        if (mkstemp(archivePath.mutableData()) == -1)
            return;

        NSDictionary *options = @{
            (__bridge id)kBOMCopierOptionCreatePKZipKey : @YES,
            (__bridge id)kBOMCopierOptionSequesterResourcesKey : @YES,
            (__bridge id)kBOMCopierOptionKeepParentKey : @YES,
            (__bridge id)kBOMCopierOptionCopyResourcesKey : @YES,
        };

        BOMCopier copier = BOMCopierNew();
        if (!BOMCopierCopyWithOptions(copier, newURL.path.fileSystemRepresentation, archivePath.data(), (__bridge CFDictionaryRef)options))
            m_replacementPath = String::fromUTF8(archivePath);
        BOMCopierFree(copier);
    }];

    m_replacementShouldBeGenerated = false;
    if (!m_replacementPath.isNull()) {
        if (auto metadata = FileSystem::fileMetadataFollowingSymlinks(m_replacementPath))
            m_size = metadata.value().length;
    }

    revokeFileAccess();
}

}

#endif
