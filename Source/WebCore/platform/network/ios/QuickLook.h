/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef QuickLook_h
#define QuickLook_h

#if USE(QUICK_LOOK)

#import "ResourceRequest.h"
#import <objc/objc-runtime.h>
#import <wtf/PassOwnPtr.h>

#ifdef __OBJC__
@class NSData;
@class NSDictionary;
@class NSFileHandle;
@class NSSet;
@class NSString;
@class NSURL;
@class NSURLConnection;
@class NSURLResponse;
#else
class NSData;
class NSDictionary;
class NSFileHandle;
class NSSet;
class NSString;
class NSURL;
class NSURLConnection;
class NSURLResponse;
#endif

#if USE(CFNETWORK)
typedef struct _CFURLResponse* CFURLResponseRef;
typedef struct _CFURLConnection* CFURLConnectionRef;
#endif

namespace WebCore {

class ResourceHandle;
class ResourceLoader;
class SynchronousResourceHandleCFURLConnectionDelegate;

Class QLPreviewConverterClass();
NSString *QLTypeCopyBestMimeTypeForFileNameAndMimeType(NSString *fileName, NSString *mimeType);
NSString *QLTypeCopyBestMimeTypeForURLAndMimeType(NSURL *, NSString *mimeType);

NSSet *QLPreviewGetSupportedMIMETypesSet();

// Used for setting the permissions on the saved QL content
NSDictionary *QLFileAttributes();
NSDictionary *QLDirectoryAttributes();

void addQLPreviewConverterWithFileForURL(NSURL *, id converter, NSString *fileName);
NSString *qlPreviewConverterFileNameForURL(NSURL *);
NSString *qlPreviewConverterUTIForURL(NSURL *);
void removeQLPreviewConverterForURL(NSURL *);

PassOwnPtr<ResourceRequest> registerQLPreviewConverterIfNeeded(NSURL *, NSString *mimeType, NSData *);

const URL safeQLURLForDocumentURLAndResourceURL(const URL& documentURL, const String& resourceURL);

const char* QLPreviewProtocol();


class QuickLookHandle {
    WTF_MAKE_NONCOPYABLE(QuickLookHandle);
public:
    static PassOwnPtr<QuickLookHandle> create(ResourceHandle*, NSURLConnection *, NSURLResponse *, id delegate);
#if USE(CFNETWORK)
    static PassOwnPtr<QuickLookHandle> create(ResourceHandle*, SynchronousResourceHandleCFURLConnectionDelegate*, CFURLResponseRef);
#endif
    static PassOwnPtr<QuickLookHandle> create(ResourceLoader*, NSURLResponse *, id delegate);
    ~QuickLookHandle();

    bool didReceiveDataArray(CFArrayRef);
    bool didReceiveData(CFDataRef);
    bool didFinishLoading();
    void didFail();

    NSURLResponse *nsResponse();
#if USE(CFNETWORK)
    CFURLResponseRef cfResponse();
#endif

private:
    QuickLookHandle(NSURL *, NSURLConnection *, NSURLResponse *, id delegate);

    RetainPtr<NSURL> m_firstRequestURL;
    RetainPtr<id> m_converter;
    RetainPtr<id> m_delegate;
    bool m_finishedLoadingDataIntoConverter;
    RetainPtr<NSFileHandle *> m_quicklookFileHandle;
    NSURLResponse *m_nsResponse;
};

} // namespace WebCore

#endif // USE(QUICK_LOOK)

#endif // QuickLook_h
