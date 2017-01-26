/*
 * Copyright (C) 2009-2017 Apple Inc. All rights reserved.
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

#if USE(QUICK_LOOK)

#include <objc/objc.h>
#include <wtf/Forward.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS NSArray;
OBJC_CLASS NSData;
OBJC_CLASS NSDictionary;
OBJC_CLASS NSSet;
OBJC_CLASS NSString;
OBJC_CLASS NSURL;
OBJC_CLASS NSURLRequest;
OBJC_CLASS NSURLResponse;
OBJC_CLASS QLPreviewConverter;
OBJC_CLASS WebPreviewConverter;

namespace WebCore {

class QuickLookHandleClient;
class ResourceLoader;
class ResourceResponse;
class SharedBuffer;
class URL;

WEBCORE_EXPORT NSSet *QLPreviewGetSupportedMIMETypesSet();

WEBCORE_EXPORT void addQLPreviewConverterWithFileForURL(NSURL *, id converter, NSString *fileName);
WEBCORE_EXPORT NSString *qlPreviewConverterFileNameForURL(NSURL *);
WEBCORE_EXPORT NSString *qlPreviewConverterUTIForURL(NSURL *);
WEBCORE_EXPORT void removeQLPreviewConverterForURL(NSURL *);

WEBCORE_EXPORT RetainPtr<NSURLRequest> registerQLPreviewConverterIfNeeded(NSURL *, NSString *mimeType, NSData *);

const URL safeQLURLForDocumentURLAndResourceURL(const URL& documentURL, const String& resourceURL);

WEBCORE_EXPORT const char* QLPreviewProtocol();

WEBCORE_EXPORT NSString *createTemporaryFileForQuickLook(NSString *fileName);

class QuickLookHandle {
    WTF_MAKE_NONCOPYABLE(QuickLookHandle);
public:
    static bool shouldCreateForMIMEType(const String&);
    static std::unique_ptr<QuickLookHandle> create(ResourceLoader&, const ResourceResponse&);
    ~QuickLookHandle();

    bool didReceiveData(const char* data, unsigned length);
    bool didReceiveBuffer(const SharedBuffer&);
    bool didFinishLoading();
    void didFail();

    WEBCORE_EXPORT void setClient(Ref<QuickLookHandleClient>&&);
    WEBCORE_EXPORT static void setClientForTesting(RefPtr<QuickLookHandleClient>&&);

    WEBCORE_EXPORT String previewFileName() const;
    WEBCORE_EXPORT String previewUTI() const;
    WEBCORE_EXPORT NSURL *previewRequestURL() const;
    WEBCORE_EXPORT QLPreviewConverter *converter() const;
    NSURL *firstRequestURL() const { return m_firstRequestURL.get(); }

private:
    friend std::unique_ptr<QuickLookHandle> std::make_unique<QuickLookHandle>(ResourceLoader&, const ResourceResponse&);
    QuickLookHandle(ResourceLoader&, const ResourceResponse&);

    RetainPtr<NSURL> m_firstRequestURL;
    RetainPtr<WebPreviewConverter> m_converter;
    bool m_finishedLoadingDataIntoConverter { false };
};

} // namespace WebCore

#endif // USE(QUICK_LOOK)
