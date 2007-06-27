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

#include "DefaultDownloadDelegate.h"
#include "MarshallingHelpers.h"
#include "WebError.h"
#include "WebKit.h"
#include "WebKitLogging.h"
#include "WebMutableURLRequest.h"
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
#include <WebCore/SystemTime.h>
#pragma warning(pop)

using namespace WebCore;

// CFURLDownload Callbacks ----------------------------------------------------------------
static void didStartCallback(CFURLDownloadRef download, const void *clientInfo);
static CFURLRequestRef willSendRequestCallback(CFURLDownloadRef download, CFURLRequestRef request, CFURLResponseRef redirectionResponse, const void *clientInfo);
static void didReceiveAuthenticationChallengeCallback(CFURLDownloadRef download, CFURLAuthChallengeRef challenge, const void *clientInfo);
static void didReceiveResponseCallback(CFURLDownloadRef download, CFURLResponseRef response, const void *clientInfo);
static void willResumeWithResponseCallback(CFURLDownloadRef download, CFURLResponseRef response, UInt64 startingByte, const void *clientInfo);
static void didReceiveDataCallback(CFURLDownloadRef download, CFIndex length, const void *clientInfo);
static Boolean shouldDecodeDataOfMIMETypeCallback(CFURLDownloadRef download, CFStringRef encodingType, const void *clientInfo);
static void decideDestinationWithSuggestedObjectNameCallback(CFURLDownloadRef download, CFStringRef objectName, const void *clientInfo);
static void didCreateDestinationCallback(CFURLDownloadRef download, CFURLRef path, const void *clientInfo);
static void didFinishCallback(CFURLDownloadRef download, const void *clientInfo);
static void didFailCallback(CFURLDownloadRef download, CFErrorRef error, const void *clientInfo);

// Download Bundle file utilities ----------------------------------------------------------------
static const String BundleExtension(".download");
static UInt32 BundleMagicNumber = 0xDECAF4EA;

static CFDataRef extractResumeDataFromBundle(const String& bundlePath);
static HRESULT appendResumeDataToBundle(CFDataRef resumeData, const String& bundlePath);

// WebDownload ----------------------------------------------------------------

WebDownload::WebDownload()
    : m_refCount(0)
{
    gClassCount++;
}

void WebDownload::init(ResourceHandle* handle, const ResourceRequest& request, const ResourceResponse& response, IWebDownloadDelegate* delegate)
{
    m_delegate = delegate ? delegate : DefaultDownloadDelegate::sharedInstance();
    CFURLConnectionRef connection = handle->connection();
    if (!connection) {
        LOG_ERROR("WebDownload::WebDownload(ResourceHandle*,...) called with an inactive ResourceHandle");    
        return;
    }

    CFURLDownloadClient client = {0, this, 0, 0, 0, didStartCallback, willSendRequestCallback, didReceiveAuthenticationChallengeCallback, 
        didReceiveResponseCallback, willResumeWithResponseCallback, didReceiveDataCallback, shouldDecodeDataOfMIMETypeCallback,
        decideDestinationWithSuggestedObjectNameCallback, didCreateDestinationCallback, didFinishCallback, didFailCallback};

    m_request.adoptRef(WebMutableURLRequest::createInstance(request));
    m_download.adoptCF(CFURLDownloadCreateAndStartWithLoadingConnection(0, connection, request.cfURLRequest(), response.cfURLResponse(), &client));

    // It is possible for CFURLDownloadCreateAndStartWithLoadingConnection() to fail if the passed in CFURLConnection is not in a "downloadable state"
    // However, we should never hit that case
    if (!m_download) {
        ASSERT_NOT_REACHED();
        LOG_ERROR("WebDownload - Failed to create WebDownload from existing connection (%s)", request.url().url().ascii());
    } else
        LOG(Download, "WebDownload - Created WebDownload %p from existing connection (%s)", this, request.url().url().ascii());

    // The CFURLDownload either starts successfully and retains the CFURLConnection, 
    // or it fails to creating and we have a now-useless connection with a dangling ref. 
    // Either way, we need to release the connection to balance out ref counts
    handle->releaseConnectionForDownload();
    CFRelease(connection);
}

void WebDownload::init(const KURL& url, IWebDownloadDelegate* delegate)
{
    m_delegate = delegate ? delegate : DefaultDownloadDelegate::sharedInstance();
    LOG_ERROR("Delegate is %p", m_delegate.get());

    ResourceRequest request(url);
    CFURLRequestRef cfRequest = request.cfURLRequest();

    CFURLDownloadClient client = {0, this, 0, 0, 0, didStartCallback, willSendRequestCallback, didReceiveAuthenticationChallengeCallback, 
                                  didReceiveResponseCallback, willResumeWithResponseCallback, didReceiveDataCallback, shouldDecodeDataOfMIMETypeCallback, 
                                  decideDestinationWithSuggestedObjectNameCallback, didCreateDestinationCallback, didFinishCallback, didFailCallback};
    m_request.adoptRef(WebMutableURLRequest::createInstance(request));
    m_download.adoptCF(CFURLDownloadCreate(0, cfRequest, &client));

    CFURLDownloadScheduleWithCurrentMessageQueue(m_download.get());
    CFURLDownloadScheduleDownloadWithRunLoop(m_download.get(), ResourceHandle::loaderRunLoop(), kCFRunLoopDefaultMode);

    LOG(Download, "WebDownload - Initialized download of url %s in WebDownload %p", url.url().ascii(), this);
}

WebDownload::~WebDownload()
{
    LOG(Download, "WebDownload - Destroying download (%p)", this);
    cancel();
    gClassCount--;
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

HRESULT STDMETHODCALLTYPE WebDownload::initWithRequest(
        /* [in] */ IWebURLRequest* request, 
        /* [in] */ IWebDownloadDelegate* delegate)
{
    COMPtr<WebMutableURLRequest> webRequest;
    if (!request || FAILED(request->QueryInterface(CLSID_WebMutableURLRequest, (void**)&webRequest))) {
        LOG(Download, "WebDownload - initWithRequest failed - not a WebMutableURLRequest");    
        return E_FAIL;
    }

    if (!delegate)
        return E_FAIL;
    m_delegate = delegate;
    LOG(Download, "Delegate is %p", m_delegate.get());

    RetainPtr<CFURLRequestRef> cfRequest = webRequest->resourceRequest().cfURLRequest();

    CFURLDownloadClient client = {0, this, 0, 0, 0, didStartCallback, willSendRequestCallback, didReceiveAuthenticationChallengeCallback, 
                                  didReceiveResponseCallback, willResumeWithResponseCallback, didReceiveDataCallback, shouldDecodeDataOfMIMETypeCallback, 
                                  decideDestinationWithSuggestedObjectNameCallback, didCreateDestinationCallback, didFinishCallback, didFailCallback};
    m_request.adoptRef(WebMutableURLRequest::createInstance(webRequest.get()));
    m_download.adoptCF(CFURLDownloadCreate(0, cfRequest.get(), &client));

    // If for some reason the download failed to create, 
    // we have particular cleanup to do
    if (!m_download) {
        m_request = 0;    
        return E_FAIL;
    }

    CFURLDownloadScheduleWithCurrentMessageQueue(m_download.get());
    CFURLDownloadScheduleDownloadWithRunLoop(m_download.get(), ResourceHandle::loaderRunLoop(), kCFRunLoopDefaultMode);

    LOG(Download, "WebDownload - initWithRequest complete, started download of url %s", webRequest->resourceRequest().url().url().ascii());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebDownload::initToResumeWithBundle(
        /* [in] */ BSTR bundlePath, 
        /* [in] */ IWebDownloadDelegate* delegate)
{
    LOG(Download, "Attempting resume of download bundle %s", String(bundlePath, SysStringLen(bundlePath)).ascii().data());

    RetainPtr<CFDataRef> resumeData(AdoptCF, extractResumeDataFromBundle(String(bundlePath, SysStringLen(bundlePath))));
    
    if (!resumeData)
        return E_FAIL;

    if (!delegate)
        return E_FAIL;
    m_delegate = delegate;
    LOG(Download, "Delegate is %p", m_delegate.get());

    CFURLDownloadClient client = {0, this, 0, 0, 0, didStartCallback, willSendRequestCallback, didReceiveAuthenticationChallengeCallback, 
                                  didReceiveResponseCallback, willResumeWithResponseCallback, didReceiveDataCallback, shouldDecodeDataOfMIMETypeCallback, 
                                  decideDestinationWithSuggestedObjectNameCallback, didCreateDestinationCallback, didFinishCallback, didFailCallback};
    
    RetainPtr<CFURLRef> pathURL(AdoptCF, MarshallingHelpers::PathStringToFileCFURLRef(String(bundlePath, SysStringLen(bundlePath))));
    ASSERT(pathURL);

    m_download.adoptCF(CFURLDownloadCreateWithResumeData(0, resumeData.get(), pathURL.get(), &client));

    if (!m_download) {
        LOG(Download, "Failed to create CFURLDownloadRef for resume");    
        return E_FAIL;
    }
    
    m_bundlePath = String(bundlePath, SysStringLen(bundlePath));
    // Attempt to remove the ".download" extension from the bundle for the final file destination
    // Failing that, we clear m_destination and will ask the delegate later once the download starts
    if (m_bundlePath.endsWith(BundleExtension, false)) {
        m_destination = m_bundlePath.copy();
        m_destination.truncate(m_destination.length() - BundleExtension.length());
    } else
        m_destination = String();
    
    CFURLDownloadScheduleWithCurrentMessageQueue(m_download.get());
    CFURLDownloadScheduleDownloadWithRunLoop(m_download.get(), ResourceHandle::loaderRunLoop(), kCFRunLoopDefaultMode);

    LOG(Download, "WebDownload - initWithRequest complete, resumed download of bundle %s", String(bundlePath, SysStringLen(bundlePath)).ascii().data());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebDownload::canResumeDownloadDecodedWithEncodingMIMEType(
        /* [in] */ BSTR, 
        /* [out, retval] */ BOOL*)
{
    notImplemented();
    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebDownload::start()
{
    LOG(Download, "WebDownload - Starting download (%p)", this);
    if (!m_download)
        return E_FAIL;

    CFURLDownloadStart(m_download.get());
    // FIXME: 4950477 - CFURLDownload neglects to make the didStart() client call upon starting the download.
    // This is a somewhat critical call, so we'll fake it for now!
    didStart();

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebDownload::cancel()
{
    LOG(Download, "WebDownload - Cancelling download (%p)", this);
    if (!m_download)
        return E_FAIL;

    CFURLDownloadCancel(m_download.get());
    m_download = 0;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebDownload::cancelForResume()
{
    LOG(Download, "WebDownload - Cancelling download (%p), writing resume information to file if possible", this);
    ASSERT(m_download);
    if (!m_download)
        return E_FAIL;

    HRESULT hr = S_OK;
    RetainPtr<CFDataRef> resumeData;
    if (m_destination.isEmpty())
        goto exit;

    CFURLDownloadSetDeletesUponFailure(m_download.get(), false);
    CFURLDownloadCancel(m_download.get());

    resumeData = CFURLDownloadCopyResumeData(m_download.get());
    if (!resumeData) {
        LOG(Download, "WebDownload - Unable to create resume data for download (%p)", this);
        goto exit;
    }

    appendResumeDataToBundle(resumeData.get(), m_bundlePath);
   
exit:
    m_download = 0;
    return hr;
}

HRESULT STDMETHODCALLTYPE WebDownload::deletesFileUponFailure(
        /* [out, retval] */ BOOL* result)
{
    if (!m_download)
        return E_FAIL;
    *result = CFURLDownloadDeletesUponFailure(m_download.get());
    return S_OK;
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

    bundle += BundleExtension;
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

HRESULT STDMETHODCALLTYPE WebDownload::setDeletesFileUponFailure(
        /* [in] */ BOOL deletesFileUponFailure)
{
    if (!m_download)
        return E_FAIL;
    CFURLDownloadSetDeletesUponFailure(m_download.get(), !!deletesFileUponFailure);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebDownload::setDestination(
        /* [in] */ BSTR path, 
        /* [in] */ BOOL allowOverwrite)
{
    if (!m_download)
        return E_FAIL;

    m_destination = String(path, SysStringLen(path));
    m_bundlePath = m_destination + BundleExtension;

    CFURLRef pathURL = MarshallingHelpers::PathStringToFileCFURLRef(m_bundlePath);
    CFURLDownloadSetDestination(m_download.get(), pathURL, !!allowOverwrite);
    CFRelease(pathURL);

#ifndef NDEBUG
    LOG(Download, "WebDownload - Set destination to %s", m_bundlePath.ascii().data());
#endif

    return S_OK;
}

// IWebURLAuthenticationChallengeSender -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebDownload::cancelAuthenticationChallenge(
        /* [in] */ IWebURLAuthenticationChallenge*)
{
    notImplemented();
    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebDownload::continueWithoutCredentialForAuthenticationChallenge(
        /* [in] */ IWebURLAuthenticationChallenge*)
{
    notImplemented();
    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebDownload::useCredential(
        /* [in] */ IWebURLCredential*, 
        /* [in] */ IWebURLAuthenticationChallenge*)
{
    notImplemented();
    return E_FAIL;
}

// CFURLDownload Callbacks -------------------------------------------------------------------
void WebDownload::didStart()
{
#ifndef NDEBUG
    m_startTime = m_dataTime = currentTime();
    m_received = 0;
    LOG(Download, "DOWNLOAD - Started %p at %.3f seconds", this, m_startTime);
#endif
    if (FAILED(m_delegate->didBegin(this)))
        LOG_ERROR("DownloadDelegate->didBegin failed");
}

CFURLRequestRef WebDownload::willSendRequest(CFURLRequestRef request, CFURLResponseRef response)
{
    COMPtr<WebMutableURLRequest> webRequest = WebMutableURLRequest::createInstance(ResourceRequest(request));
    COMPtr<WebURLResponse> webResponse = WebURLResponse::createInstance(ResourceResponse(response));
    COMPtr<IWebMutableURLRequest> finalRequest;

    if (FAILED(m_delegate->willSendRequest(this, webRequest.get(), webResponse.get(), &finalRequest)))
        LOG_ERROR("DownloadDelegate->willSendRequest failed");
    
    if (!finalRequest)
        return 0;

    COMPtr<WebMutableURLRequest> finalWebRequest = WebMutableURLRequest::createInstance(finalRequest.get());
    m_request = finalWebRequest.get();
    return finalWebRequest->resourceRequest().cfURLRequest();
}

void WebDownload::didReceiveAuthenticationChallenge(CFURLAuthChallengeRef )
{
    notImplemented();
}

void WebDownload::didReceiveResponse(CFURLResponseRef response)
{
    COMPtr<WebURLResponse> webResponse = WebURLResponse::createInstance(ResourceResponse(response));
    if (FAILED(m_delegate->didReceiveResponse(this, webResponse.get())))
        LOG_ERROR("DownloadDelegate->didReceiveResponse failed");
}

void WebDownload::willResumeWithResponse(CFURLResponseRef response, UInt64 fromByte)
{
    COMPtr<WebURLResponse> webResponse = WebURLResponse::createInstance(ResourceResponse(response));
    if (FAILED(m_delegate->willResumeWithResponse(this, webResponse.get(), fromByte)))
        LOG_ERROR("DownloadDelegate->willResumeWithResponse failed");
}

void WebDownload::didReceiveData(CFIndex length)
{
#ifndef NDEBUG
    m_received += length;
    double current = currentTime();
    if (current - m_dataTime > 2.0)
        LOG(Download, "DOWNLOAD - %p hanged for %.3f seconds - Received %i bytes for a total of %i", this, current - m_dataTime, length, m_received);
    m_dataTime = current;
#endif
    if (FAILED(m_delegate->didReceiveDataOfLength(this, length)))
        LOG_ERROR("DownloadDelegate->didReceiveData failed");
}

bool WebDownload::shouldDecodeDataOfMIMEType(CFStringRef mimeType)
{
    BOOL result;
    if (FAILED(m_delegate->shouldDecodeSourceDataOfMIMEType(this, BString(mimeType), &result))) {
        LOG_ERROR("DownloadDelegate->shouldDecodeSourceDataOfMIMEType failed");
        return false;
    }
    return !!result;
}

void WebDownload::decideDestinationWithSuggestedObjectName(CFStringRef name)
{    
    if (FAILED(m_delegate->decideDestinationWithSuggestedFilename(this, BString(name))))
        LOG_ERROR("DownloadDelegate->decideDestinationWithSuggestedObjectName failed");
}

void WebDownload::didCreateDestination(CFURLRef destination)
{
    // The concept of the ".download bundle" is internal to the WebDownload, so therefore
    // we try to mask the delegate from its existence as much as possible by telling it the final
    // destination was created, when in reality the bundle was created

    String createdDestination = MarshallingHelpers::FileCFURLRefToPathString(destination);

    // At this point in receiving CFURLDownload callbacks, we should definitely have the bundle path stored locally
    // and it should match with the file that CFURLDownload created
    ASSERT(createdDestination == m_bundlePath);
    // And we should also always have the final-destination stored
    ASSERT(!m_destination.isEmpty());

    BString path(m_destination);
    if (FAILED(m_delegate->didCreateDestination(this, path)))
        LOG_ERROR("DownloadDelegate->didCreateDestination failed");
}

void WebDownload::didFinish()
{
#ifndef NDEBUG
    LOG(Download, "DOWNLOAD - Finished %p after %i bytes and %.3f seconds", this, m_received, currentTime() - m_startTime);
#endif

    ASSERT(!m_bundlePath.isEmpty() && !m_destination.isEmpty());
    LOG(Download, "WebDownload - Moving file from bundle %s to destination %s", m_bundlePath.ascii().data(), m_destination.ascii().data());

    // We try to rename the bundle to the final file name.  If that fails, we give the delegate one more chance to chose
    // the final file name, then we just leave it
    if (!MoveFileEx(m_bundlePath.charactersWithNullTermination(), m_destination.charactersWithNullTermination(), 0)) {
        LOG_ERROR("Failed to move bundle %s to %s on completion\nError - %i", m_bundlePath.ascii().data(), m_destination.ascii().data(), GetLastError());
        
        bool reportBundlePathAsFinalPath = true;

        BString destinationBSTR(m_destination.characters(), m_destination.length());
        if (FAILED(m_delegate->decideDestinationWithSuggestedFilename(this, destinationBSTR)))
            LOG_ERROR("delegate->decideDestinationWithSuggestedFilename() failed");

        // The call to m_delegate->decideDestinationWithSuggestedFilename() should have changed our destination, so we'll try the move
        // one last time.
        if (!m_destination.isEmpty())
            if (MoveFileEx(m_bundlePath.charactersWithNullTermination(), m_destination.charactersWithNullTermination(), 0))
                reportBundlePathAsFinalPath = false;

        // We either need to tell the delegate our final filename is the bundle filename, or is the file name they just told us to use
        if (reportBundlePathAsFinalPath) {
            BString bundleBSTR(m_bundlePath);
            m_delegate->didCreateDestination(this, bundleBSTR);
        } else {
            BString finalDestinationBSTR = BString(m_destination);
            m_delegate->didCreateDestination(this, finalDestinationBSTR);
        }
    }

    // It's extremely likely the call to delegate->didFinish() will deref this, so lets not let that cause our destruction just yet
    COMPtr<WebDownload> protect = this;
    if (FAILED(m_delegate->didFinish(this)))
        LOG_ERROR("DownloadDelegate->didFinish failed");

    m_download = 0;
}

void WebDownload::didFail(CFErrorRef error)
{
    COMPtr<WebError> webError = WebError::createInstance(ResourceError(error));
    if (FAILED(m_delegate->didFailWithError(this, webError.get())))
        LOG_ERROR("DownloadDelegate->didFailWithError failed");
}

// CFURLDownload Callbacks ----------------------------------------------------------------
void didStartCallback(CFURLDownloadRef, const void *clientInfo)
{ ((WebDownload*)clientInfo)->didStart(); }

CFURLRequestRef willSendRequestCallback(CFURLDownloadRef, CFURLRequestRef request, CFURLResponseRef redirectionResponse, const void *clientInfo)
{ return ((WebDownload*)clientInfo)->willSendRequest(request, redirectionResponse); }

void didReceiveAuthenticationChallengeCallback(CFURLDownloadRef, CFURLAuthChallengeRef challenge, const void *clientInfo)
{ ((WebDownload*)clientInfo)->didReceiveAuthenticationChallenge(challenge); }

void didReceiveResponseCallback(CFURLDownloadRef, CFURLResponseRef response, const void *clientInfo)
{ ((WebDownload*)clientInfo)->didReceiveResponse(response); }

void willResumeWithResponseCallback(CFURLDownloadRef, CFURLResponseRef response, UInt64 startingByte, const void *clientInfo)
{ ((WebDownload*)clientInfo)->willResumeWithResponse(response, startingByte); }

void didReceiveDataCallback(CFURLDownloadRef, CFIndex length, const void *clientInfo)
{ ((WebDownload*)clientInfo)->didReceiveData(length); }

Boolean shouldDecodeDataOfMIMETypeCallback(CFURLDownloadRef, CFStringRef encodingType, const void *clientInfo)
{ return ((WebDownload*)clientInfo)->shouldDecodeDataOfMIMEType(encodingType); }

void decideDestinationWithSuggestedObjectNameCallback(CFURLDownloadRef, CFStringRef objectName, const void *clientInfo)
{ ((WebDownload*)clientInfo)->decideDestinationWithSuggestedObjectName(objectName); }

void didCreateDestinationCallback(CFURLDownloadRef, CFURLRef path, const void *clientInfo)
{ ((WebDownload*)clientInfo)->didCreateDestination(path); }

void didFinishCallback(CFURLDownloadRef, const void *clientInfo)
{ ((WebDownload*)clientInfo)->didFinish(); }

void didFailCallback(CFURLDownloadRef, CFErrorRef error, const void *clientInfo)
{ ((WebDownload*)clientInfo)->didFail(error); }

// Download Bundle file utilities ----------------------------------------------------------------

static CFDataRef extractResumeDataFromBundle(const String& bundlePath)
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

    if (footerMagicNumber != BundleMagicNumber) {
        LOG_ERROR("Footer's magic number does not match 0x%X - errno(%i)", BundleMagicNumber, errno);  
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

static HRESULT appendResumeDataToBundle(CFDataRef resumeData, const String& bundlePath)
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

    if (fwrite(&BundleMagicNumber, 4, 1, bundle) != 1) {
        LOG_ERROR("Failed to write footer magic number to the bundle - errno(%i)", errno);
        goto exit;
    }
    
    hr = S_OK;
exit:
    fclose(bundle);
    return hr;
}
