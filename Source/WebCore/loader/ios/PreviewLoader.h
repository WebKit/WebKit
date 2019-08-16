/*
 * Copyright (C) 2009-2019 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS WebPreviewLoader;

namespace WebCore {

class PreviewLoaderClient;
class ResourceLoader;
class ResourceResponse;
class SharedBuffer;

class PreviewLoader {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(PreviewLoader);
public:
    static std::unique_ptr<PreviewLoader> create(ResourceLoader&, const ResourceResponse&);
    ~PreviewLoader();

    bool didReceiveResponse(const ResourceResponse&);
    bool didReceiveData(const char* data, unsigned length);
    bool didReceiveBuffer(const SharedBuffer&);
    bool didFinishLoading();
    void didFail();

    WEBCORE_EXPORT static void setClientForTesting(RefPtr<PreviewLoaderClient>&&);

private:
    friend std::unique_ptr<PreviewLoader> std::make_unique<PreviewLoader>(ResourceLoader&, const ResourceResponse&);
    PreviewLoader(ResourceLoader&, const ResourceResponse&);

    RetainPtr<WebPreviewLoader> m_previewLoader;
    bool m_finishedLoadingDataIntoConverter { false };
};

} // namespace WebCore
