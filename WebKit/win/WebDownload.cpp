/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "WebKitDLL.h"
#include "WebDownload.h"

#include "CString.h"
#include "DefaultDownloadDelegate.h"
#include "MarshallingHelpers.h"
#include "WebError.h"
#include "WebKit.h"
#include "WebKitLogging.h"
#include "WebMutableURLRequest.h"
#include "WebURLAuthenticationChallenge.h"
#include "WebURLCredential.h"
#include "WebURLResponse.h"

#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>

#pragma warning(push, 0)
#include <WebCore/BString.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceHandle.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <wtf/CurrentTime.h>
#include <wtf/StdLibExtras.h>
#pragma warning(pop)

using namespace WebCore;

// Download Bundle file utilities ----------------------------------------------------------------
const String& WebDownload::bundleExtension()
{
   DEFINE_STATIC_LOCAL(const String, bundleExtension, (".download"));
   return bundleExtension;
}

UInt32 WebDownload::bundleMagicNumber()
{
   return 0xDECAF4EA;
}

// WebDownload ----------------------------------------------------------------

WebDownload::WebDownload()
    : m_refCount(0)
{
    gClassCount++;
    gClassNameCount.add("WebDownload");
}

WebDownload::~WebDownload()
{
    LOG(Download, "WebDownload - Destroying download (%p)", this);
    cancel();
    gClassCount--;
    gClassNameCount.remove("WebDownload");
}

WebDownload* WebDownload::createInstance()
{
    WebDownload* instance = new WebDownload();
    instance->AddRef();
    return instance;
}

WebDownload* WebDownload::createInstance(ResourceHandle* handle, const ResourceRequest& request, const ResourceResponse& response, IWebDownloadDelegate* delegate)
{
    WebDownload* instance = new WebDownload();
    instance->AddRef();
    instance->init(handle, request, response, delegate);
    return instance;
}

WebDownload* WebDownload::createInstance(const KURL& url, IWebDownloadDelegate* delegate)
{
    WebDownload* instance = new WebDownload();
    instance->AddRef();
    instance->init(url, delegate);
    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebDownload::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebDownload*>(this);
    else if (IsEqualGUID(riid, IID_IWebDownload))
        *ppvObject = static_cast<IWebDownload*>(this);
    else if (IsEqualGUID(riid, IID_IWebURLAuthenticationChallengeSender))
        *ppvObject = static_cast<IWebURLAuthenticationChallengeSender*>(this);
    else if (IsEqualGUID(riid, CLSID_WebDownload))
        *ppvObject = static_cast<WebDownload*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebDownload::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebDownload::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebDownload -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebDownload::canResumeDownloadDecodedWithEncodingMIMEType(
        /* [in] */ BSTR, 
        /* [out, retval] */ BOOL*)
{
    notImplemented();
    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebDownload::bundlePathForTargetPath(
        /* [in] */ BSTR targetPath, 
        /* [out, retval] */ BSTR* bundlePath)
{
    if (!targetPath)
        return E_INVALIDARG;

    String bundle(targetPath, SysStringLen(targetPath));
    if (bundle.isEmpty())
        return E_INVALIDARG;

    if (bundle[bundle.length()-1] == '/')
        bundle.truncate(1);

    bundle += bundleExtension();
    *bundlePath = SysAllocStringLen(bundle.characters(), bundle.length());
    if (!*bundlePath)
       return E_FAIL;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebDownload::request(
        /* [out, retval] */ IWebURLRequest** request)
{
    if (request) {
        *request = m_request.get();
        if (*request)
            (*request)->AddRef();
    }
    return S_OK;
}

// Download Bundle file utilities ----------------------------------------------------------------

CFDataRef WebDownload::extractResumeDataFromBundle(const String& bundlePath)
{
    if (bundlePath.isEmpty()) {
        LOG_ERROR("Cannot create resume data from empty download bundle path");
        return 0;
    }
    
    // Open a handle to the bundle file
    String nullifiedPath = bundlePath;
    FILE* bundle = 0;
    if (_wfopen_s(&bundle, nullifiedPath.charactersWithNullTermination(), TEXT("r+b")) || !bundle) {
        LOG_ERROR("Failed to open file %s to get resume data", bundlePath.ascii().data());
        return 0;
    }

    CFDataRef result = 0;
    Vector<UInt8> footerBuffer;
    
    // Stat the file to get its size
    struct _stat64 fileStat;
    if (_fstat64(_fileno(bundle), &fileStat))
        goto exit;
    
    // Check for the bundle magic number at the end of the file
    fpos_t footerMagicNumberPosition = fileStat.st_size - 4;
    ASSERT(footerMagicNumberPosition >= 0);
    if (footerMagicNumberPosition < 0)
        goto exit;
    if (fsetpos(bundle, &footerMagicNumberPosition))
        goto exit;

    UInt32 footerMagicNumber = 0;
    if (fread(&footerMagicNumber, 4, 1, bundle) != 1) {
        LOG_ERROR("Failed to read footer magic number from the bundle - errno(%i)", errno);  
        goto exit;
    }

    if (footerMagicNumber != bundleMagicNumber()) {
        LOG_ERROR("Footer's magic number does not match 0x%X - errno(%i)", bundleMagicNumber(), errno);  
        goto exit;
    }

    // Now we're *reasonably* sure this is a .download bundle we actually wrote. 
    // Get the length of the resume data
    fpos_t footerLengthPosition = fileStat.st_size - 8;
    ASSERT(footerLengthPosition >= 0);
    if (footerLengthPosition < 0)
        goto exit;

    if (fsetpos(bundle, &footerLengthPosition))
        goto exit;

    UInt32 footerLength = 0;
    if (fread(&footerLength, 4, 1, bundle) != 1) {
        LOG_ERROR("Failed to read ResumeData length from the bundle - errno(%i)", errno);  
        goto exit;
    }

    // Make sure theres enough bytes to read in for the resume data, and perform the read
    fpos_t footerStartPosition = fileStat.st_size - 8 - footerLength;
    ASSERT(footerStartPosition >= 0);
    if (footerStartPosition < 0)
        goto exit;
    if (fsetpos(bundle, &footerStartPosition))
        goto exit;

    footerBuffer.resize(footerLength);
    if (fread(footerBuffer.data(), 1, footerLength, bundle) != footerLength) {
        LOG_ERROR("Failed to read ResumeData from the bundle - errno(%i)", errno);  
        goto exit;
    }

    // CFURLDownload will seek to the appropriate place in the file (before our footer) and start overwriting from there
    // However, say we were within a few hundred bytes of the end of a download when it was paused -
    // The additional footer extended the length of the file beyond its final length, and there will be junk data leftover
    // at the end.  Therefore, now that we've retrieved the footer data, we need to truncate it.
    if (errno_t resizeError = _chsize_s(_fileno(bundle), footerStartPosition)) {
        LOG_ERROR("Failed to truncate the resume footer off the end of the file - errno(%i)", resizeError);
        goto exit;
    }

    // Finally, make the resume data.  Now, it is possible by some twist of fate the bundle magic number
    // was naturally at the end of the file and its not actually a valid bundle.  That, or someone engineered
    // it that way to try to attack us.  In that cause, this CFData will successfully create but when we 
    // actually try to start the CFURLDownload using this bogus data, it will fail and we will handle that gracefully
    result = CFDataCreate(0, footerBuffer.data(), footerLength);
exit:
    fclose(bundle);
    return result;
}

HRESULT WebDownload::appendResumeDataToBundle(CFDataRef resumeData, const String& bundlePath)
{
    if (!resumeData) {
        LOG_ERROR("Invalid resume data to write to bundle path");
        return E_FAIL;
    }
    if (bundlePath.isEmpty()) {
        LOG_ERROR("Cannot write resume data to empty download bundle path");
        return E_FAIL;
    }

    String nullifiedPath = bundlePath;
    FILE* bundle = 0;
    if (_wfopen_s(&bundle, nullifiedPath.charactersWithNullTermination(), TEXT("ab")) || !bundle) {
        LOG_ERROR("Failed to open file %s to append resume data", bundlePath.ascii().data());
        return E_FAIL;
    }

    HRESULT hr = E_FAIL;

    const UInt8* resumeBytes = CFDataGetBytePtr(resumeData);
    ASSERT(resumeBytes);
    if (!resumeBytes)
        goto exit;

    UInt32 resumeLength = CFDataGetLength(resumeData);
    ASSERT(resumeLength > 0);
    if (resumeLength < 1)
        goto exit;

    if (fwrite(resumeBytes, 1, resumeLength, bundle) != resumeLength) {
        LOG_ERROR("Failed to write resume data to the bundle - errno(%i)", errno);
        goto exit;
    }

    if (fwrite(&resumeLength, 4, 1, bundle) != 1) {
        LOG_ERROR("Failed to write footer length to the bundle - errno(%i)", errno);
        goto exit;
    }

    const UInt32& magic = bundleMagicNumber();
    if (fwrite(&magic, 4, 1, bundle) != 1) {
        LOG_ERROR("Failed to write footer magic number to the bundle - errno(%i)", errno);
        goto exit;
    }
    
    hr = S_OK;
exit:
    fclose(bundle);
    return hr;
}
