/*
 * Copyright (C) 2009, 2010, 2011 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "NetworkJob.h"

#include "AboutData.h"
#include "Base64.h"
#include "CookieManager.h"
#include "CredentialStorage.h"
#include "Frame.h"
#include "FrameLoaderClientBlackBerry.h"
#include "HTTPParsers.h"
#include "KURL.h"
#include "MIMETypeRegistry.h"
#include "NetworkManager.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "ResourceRequest.h"

#include <BlackBerryPlatformClient.h>
#include <BlackBerryPlatformLog.h>
#include <BlackBerryPlatformWebKitCredits.h>
#include <BuildInformation.h>
#include <network/MultipartStream.h>
#include <network/NetworkRequest.h>
#include <network/NetworkStreamFactory.h>
#include <wtf/ASCIICType.h>

namespace WebCore {

static const int s_redirectMaximum = 10;

inline static bool isInfo(int statusCode)
{
    return 100 <= statusCode && statusCode < 200;
}

inline static bool isRedirect(int statusCode)
{
    return 300 <= statusCode && statusCode < 400 && statusCode != 304;
}

inline static bool isUnauthorized(int statusCode)
{
    return statusCode == 401;
}

static void escapeDecode(const char* src, int length, Vector<char>& out)
{
    out.resize(length);
    const char* const srcEnd = src + length;
    char* dst = out.data();
    for (; src < srcEnd; ) {
        char inputChar = *src++;
        if (inputChar == '%' && src + 2 <= srcEnd) {
            int digit1 = 0;
            char character = *src++;
            if (isASCIIHexDigit(character))
                digit1 = toASCIIHexValue(character);

            int digit2 = 0;
            character = *src++;
            if (isASCIIHexDigit(character))
                digit2 = toASCIIHexValue(character);

            *dst++ = (digit1 << 4) | digit2;
        } else
            *dst++ = inputChar;
    }
    out.resize(dst - out.data());
}

NetworkJob::NetworkJob()
    : m_playerId(0)
    , m_loadDataTimer(this, &NetworkJob::fireLoadDataTimer)
    , m_loadAboutTimer(this, &NetworkJob::fireLoadAboutTimer)
    , m_deleteJobTimer(this, &NetworkJob::fireDeleteJobTimer)
    , m_streamFactory(0)
    , m_isFile(false)
    , m_isData(false)
    , m_isAbout(false)
    , m_isFTP(false)
    , m_isFTPDir(true)
#ifndef NDEBUG
    , m_isRunning(true) // Always started immediately after creation.
#endif
    , m_cancelled(false)
    , m_statusReceived(false)
    , m_dataReceived(false)
    , m_responseSent(false)
    , m_callingClient(false)
    , m_isXHR(false)
    , m_needsRetryAsFTPDirectory(false)
    , m_extendedStatusCode(0)
    , m_redirectCount(0)
    , m_deferredData(*this)
    , m_deferLoadingCount(0)
{
}

bool NetworkJob::initialize(int playerId,
                            const String& pageGroupName,
                            const KURL& url,
                            const BlackBerry::Platform::NetworkRequest& request,
                            PassRefPtr<ResourceHandle> handle,
                            BlackBerry::Platform::NetworkStreamFactory* streamFactory,
                            const Frame& frame,
                            int deferLoadingCount,
                            int redirectCount)
{
    m_playerId = playerId;
    m_pageGroupName = pageGroupName;

    m_response.setURL(url);
    m_isFile = url.protocolIs("file") || url.protocolIs("local");
    m_isData = url.protocolIs("data");
    m_isAbout = url.protocolIs("about");
    m_isFTP = url.protocolIs("ftp");

    m_handle = handle;

    m_streamFactory = streamFactory;
    m_frame = &frame;
    m_redirectCount = redirectCount;
    m_deferLoadingCount = deferLoadingCount;

    // We don't need to explicitly call notifyHeaderReceived, as the Content-Type
    // will ultimately get parsed when sendResponseIfNeeded gets called.
    if (!request.getOverrideContentType().empty())
        m_contentType = String(request.getOverrideContentType().c_str());

    // No need to create the streams for data and about.
    if (m_isData || m_isAbout)
        return true;

    BlackBerry::Platform::FilterStream* wrappedStream = m_streamFactory->createNetworkStream(request, m_playerId);
    if (!wrappedStream)
        return false;
    setWrappedStream(wrappedStream);

    m_isXHR = request.getTargetType() == BlackBerry::Platform::NetworkRequest::TargetIsXMLHTTPRequest;

    return true;
}

void NetworkJob::loadAboutURL()
{
    m_loadAboutTimer.startOneShot(0);
}

int NetworkJob::cancelJob()
{
    m_cancelled = true;

    // Cancel jobs loading local data by killing the timer, and jobs
    // getting data from the network by calling the inherited URLStream::cancel.
    if (m_loadDataTimer.isActive()) {
        m_loadDataTimer.stop();
        notifyClose(BlackBerry::Platform::FilterStream::StatusCancelled);
        return 0;
    }

    if (m_loadAboutTimer.isActive()) {
        m_loadAboutTimer.stop();
        notifyClose(BlackBerry::Platform::FilterStream::StatusCancelled);
        return 0;
    }

    return streamCancel();
}

void NetworkJob::updateDeferLoadingCount(int delta)
{
    m_deferLoadingCount += delta;
    ASSERT(m_deferLoadingCount >= 0);

    if (!isDeferringLoading()) {
        // There might already be a timer set to call this, but it's safe to schedule it again.
        m_deferredData.scheduleProcessDeferredData();
    }
}

void NetworkJob::notifyStatusReceived(int status, const char* message)
{
    if (shouldDeferLoading())
        m_deferredData.deferOpen(status, message);
    else
        handleNotifyStatusReceived(status, message);
}

void NetworkJob::handleNotifyStatusReceived(int status, const String& message)
{
    // Check for messages out of order or after cancel.
    if ((m_statusReceived && m_extendedStatusCode != 401) || m_responseSent || m_cancelled)
        return;

    if (isInfo(status))
        return; // ignore

    m_statusReceived = true;

    // Convert non-HTTP status codes to generic HTTP codes.
    m_extendedStatusCode = status;
    if (!status)
        m_response.setHTTPStatusCode(200);
    else if (status < 0)
        m_response.setHTTPStatusCode(404);
    else
        m_response.setHTTPStatusCode(status);

    m_response.setHTTPStatusText(message);
}

void NetworkJob::notifyWMLOverride()
{
    if (shouldDeferLoading())
        m_deferredData.deferWMLOverride();
    else
        handleNotifyWMLOverride();
}

void NetworkJob::notifyHeaderReceived(const char* key, const char* value)
{
    if (shouldDeferLoading())
        m_deferredData.deferHeaderReceived(key, value);
    else {
        String keyString(key);
        String valueString;
        if (equalIgnoringCase(keyString, "Location")) {
            // Location, like all headers, is supposed to be Latin-1. But some sites (wikipedia) send it in UTF-8.
            // All byte strings that are valid UTF-8 are also valid Latin-1 (although outside ASCII, the meaning will
            // differ), but the reverse isn't true. So try UTF-8 first and fall back to Latin-1 if it's invalid.
            // (High Latin-1 should be url-encoded anyway.)
            //
            // FIXME: maybe we should do this with other headers?
            // Skip it for now - we don't want to rewrite random bytes unless we're sure. (Definitely don't want to
            // rewrite cookies, for instance.) Needs more investigation.
            valueString = String::fromUTF8(value);
            if (valueString.isNull())
                valueString = value;
        } else
            valueString = value;

        handleNotifyHeaderReceived(keyString, valueString);
    }
}

void NetworkJob::notifyMultipartHeaderReceived(const char* key, const char* value)
{
    if (shouldDeferLoading())
        m_deferredData.deferMultipartHeaderReceived(key, value);
    else
        handleNotifyMultipartHeaderReceived(key, value);
}

void NetworkJob::notifyStringHeaderReceived(const String& key, const String& value)
{
    if (shouldDeferLoading())
        m_deferredData.deferHeaderReceived(key, value);
    else
        handleNotifyHeaderReceived(key, value);
}

void NetworkJob::handleNotifyHeaderReceived(const String& key, const String& value)
{
    // Check for messages out of order or after cancel.
    if (!m_statusReceived || m_responseSent || m_cancelled)
        return;

    String lowerKey = key.lower();
    if (lowerKey == "content-type")
        m_contentType = value.lower();

    if (lowerKey == "content-disposition")
        m_contentDisposition = value;

    if (lowerKey == "set-cookie") {
        if (m_frame && m_frame->loader() && m_frame->loader()->client()
            && static_cast<FrameLoaderClientBlackBerry*>(m_frame->loader()->client())->cookiesEnabled())
            handleSetCookieHeader(value);
    }

    if (lowerKey == "www-authenticate")
        handleAuthHeader(ProtectionSpaceServerHTTP, value);
    else if (lowerKey == "proxy-authenticate" && !BlackBerry::Platform::Client::get()->getProxyAddress().empty())
        handleAuthHeader(ProtectionSpaceProxyHTTP, value);

    if (equalIgnoringCase(key, BlackBerry::Platform::NetworkRequest::HEADER_BLACKBERRY_FTP))
        handleFTPHeader(value);

    m_response.setHTTPHeaderField(key, value);
}

void NetworkJob::handleNotifyMultipartHeaderReceived(const String& key, const String& value)
{
    if (!m_multipartResponse) {
        // Create a new response based on the original set of headers + the
        // replacement headers. We only replace the same few headers that gecko
        // does. See netwerk/streamconv/converters/nsMultiMixedConv.cpp.
        m_multipartResponse = adoptPtr(new ResourceResponse);
        m_multipartResponse->setURL(m_response.url());

        // The list of BlackBerry::Platform::replaceHeaders that we do not copy from the original
        // response when generating a response.
        const WebCore::HTTPHeaderMap& map = m_response.httpHeaderFields();

        for (WebCore::HTTPHeaderMap::const_iterator it = map.begin(); it != map.end(); ++it) {
            bool needsCopyfromOriginalResponse = true;
            int replaceHeadersIndex = 0;
            while (BlackBerry::Platform::MultipartStream::replaceHeaders[replaceHeadersIndex]) {
                if (it->first.lower() == BlackBerry::Platform::MultipartStream::replaceHeaders[replaceHeadersIndex]) {
                    needsCopyfromOriginalResponse = false;
                    break;
                }
                replaceHeadersIndex++;
            }
            if (needsCopyfromOriginalResponse)
                m_multipartResponse->setHTTPHeaderField(it->first, it->second);
        }

        m_multipartResponse->setIsMultipartPayload(true);
    } else {
        if (key.lower() == "content-type") {
            String contentType = value.lower();
            m_multipartResponse->setMimeType(extractMIMETypeFromMediaType(contentType));
            m_multipartResponse->setTextEncodingName(extractCharsetFromMediaType(contentType));
        }
        m_multipartResponse->setHTTPHeaderField(key, value);
    }
}

void NetworkJob::handleSetCookieHeader(const String& value)
{
    KURL url = m_response.url();
    CookieManager& manager = cookieManager();
    if ((manager.cookiePolicy() == CookieStorageAcceptPolicyOnlyFromMainDocumentDomain)
      && (m_handle->firstRequest().firstPartyForCookies() != url)
      && manager.getCookie(url, WithHttpOnlyCookies).isEmpty())
        return;
    manager.setCookies(url, value);
}

void NetworkJob::notifyDataReceivedPlain(const char* buf, size_t len)
{
    if (shouldDeferLoading())
        m_deferredData.deferDataReceived(buf, len);
    else
        handleNotifyDataReceived(buf, len);
}

void NetworkJob::handleNotifyDataReceived(const char* buf, size_t len)
{
    // Check for messages out of order or after cancel.
    if ((!m_isFile && !m_statusReceived) || m_cancelled)
        return;

    if (!buf || !len)
        return;

    m_dataReceived = true;

    // Protect against reentrancy.
    updateDeferLoadingCount(1);

    if (shouldSendClientData()) {
        sendResponseIfNeeded();
        sendMultipartResponseIfNeeded();
        if (isClientAvailable()) {
            RecursionGuard guard(m_callingClient);
            m_handle->client()->didReceiveData(m_handle.get(), buf, len, len);
        }
    }

    updateDeferLoadingCount(-1);
}

void NetworkJob::notifyDataSent(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    if (shouldDeferLoading())
        m_deferredData.deferDataSent(bytesSent, totalBytesToBeSent);
    else
        handleNotifyDataSent(bytesSent, totalBytesToBeSent);
}

void NetworkJob::handleNotifyDataSent(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    if (m_cancelled)
        return;

    // Protect against reentrancy.
    updateDeferLoadingCount(1);

    if (isClientAvailable()) {
        RecursionGuard guard(m_callingClient);
        m_handle->client()->didSendData(m_handle.get(), bytesSent, totalBytesToBeSent);
    }

    updateDeferLoadingCount(-1);
}

void NetworkJob::notifyClose(int status)
{
    if (shouldDeferLoading())
        m_deferredData.deferClose(status);
    else
        handleNotifyClose(status);
}

void NetworkJob::handleNotifyClose(int status)
{
#ifndef NDEBUG
    m_isRunning = false;
#endif
    if (!m_cancelled) {
        if (!m_statusReceived) {
            // Connection failed before sending notifyStatusReceived: use generic NetworkError.
            notifyStatusReceived(BlackBerry::Platform::FilterStream::StatusNetworkError, 0);
        }

        // If an HTTP authentication-enabled request is successful, save
        // the credentials for later reuse. If the request fails, delete
        // the saved credentials.
        if (!isError(m_extendedStatusCode))
            storeCredentials();
        else if (isUnauthorized(m_extendedStatusCode))
            purgeCredentials();

        if (shouldNotifyClientFinished()) {
            if (isRedirect(m_extendedStatusCode) && (m_redirectCount >= s_redirectMaximum))
                m_extendedStatusCode = BlackBerry::Platform::FilterStream::StatusTooManyRedirects;

            sendResponseIfNeeded();
            if (isClientAvailable()) {

                RecursionGuard guard(m_callingClient);
                if (isError(m_extendedStatusCode) && !m_dataReceived) {
                    String domain = m_extendedStatusCode < 0 ? ResourceError::platformErrorDomain : ResourceError::httpErrorDomain;
                    ResourceError error(domain, m_extendedStatusCode, m_response.url().string(), m_response.httpStatusText());
                    m_handle->client()->didFail(m_handle.get(), error);
                } else
                    m_handle->client()->didFinishLoading(m_handle.get(), 0);
            }
        }
    }

    // Whoever called notifyClose still have a reference to the job, so
    // schedule the deletion with a timer.
    m_deleteJobTimer.startOneShot(0);

    // Detach from the ResourceHandle in any case.
    m_handle = 0;
    m_multipartResponse = nullptr;
}

bool NetworkJob::shouldNotifyClientFinished()
{
    if (m_redirectCount >= s_redirectMaximum)
        return true;

    if (m_needsRetryAsFTPDirectory && retryAsFTPDirectory())
        return false;

    if (isRedirect(m_extendedStatusCode) && handleRedirect())
        return false;

    return true;
}

bool NetworkJob::retryAsFTPDirectory()
{
    m_needsRetryAsFTPDirectory = false;

    ASSERT(m_handle);
    ResourceRequest newRequest = m_handle->firstRequest();
    KURL url = newRequest.url();
    url.setPath(url.path() + "/");
    newRequest.setURL(url);
    newRequest.setMustHandleInternally(true);

    // Update the UI.
    handleNotifyHeaderReceived("Location", url.string());

    return startNewJobWithRequest(newRequest);
}

bool NetworkJob::startNewJobWithRequest(ResourceRequest& newRequest, bool increasRedirectCount)
{
    if (isClientAvailable()) {
        RecursionGuard guard(m_callingClient);
        m_handle->client()->willSendRequest(m_handle.get(), newRequest, m_response);

        // m_cancelled can become true if the url fails the policy check.
        if (m_cancelled)
            return false;
    }

    // Pass the ownership of the ResourceHandle to the new NetworkJob.
    RefPtr<ResourceHandle> handle = m_handle;
    m_handle = 0;
    m_multipartResponse = nullptr;

    NetworkManager::instance()->startJob(m_playerId,
        m_pageGroupName,
        handle,
        newRequest,
        m_streamFactory,
        *m_frame,
        m_deferLoadingCount,
        increasRedirectCount ? m_redirectCount + 1 : m_redirectCount);
    return true;
}

bool NetworkJob::handleRedirect()
{
    ASSERT(m_handle);
    if (!m_handle)
        return false;

    String location = m_response.httpHeaderField("Location");
    if (location.isNull())
        return false;

    KURL newURL(m_response.url(), location);
    if (!newURL.isValid())
        return false;

    ResourceRequest newRequest = m_handle->firstRequest();
    newRequest.setURL(newURL);
    newRequest.setMustHandleInternally(true);

    String method = newRequest.httpMethod().upper();
    if ((method != "GET") && (method != "HEAD")) {
        newRequest.setHTTPMethod("GET");
        newRequest.setHTTPBody(0);
        newRequest.setHTTPHeaderField("Content-Length", String());
        newRequest.setHTTPHeaderField("Content-Type", String());
    }

    // Do not send existing credentials with the new request.
    m_handle->getInternal()->m_currentWebChallenge.nullify();

    return startNewJobWithRequest(newRequest, true);
}

void NetworkJob::sendResponseIfNeeded()
{
    if (m_responseSent)
        return;

    m_responseSent = true;

    if (isError(m_extendedStatusCode) && !m_dataReceived)
        return;

    String urlFilename = m_response.url().lastPathComponent();

    // Get the MIME type that was set by the content sniffer
    // if there's no custom sniffer header, try to set it from the Content-Type header
    // if this fails, guess it from extension.
    String mimeType;
    if (m_isFTP && m_isFTPDir)
        mimeType = "application/x-ftp-directory";
    else
        mimeType = m_response.httpHeaderField(BlackBerry::Platform::NetworkRequest::HEADER_RIM_SNIFFED_MIME_TYPE);
    if (mimeType.isNull())
        mimeType = extractMIMETypeFromMediaType(m_contentType);
    if (mimeType.isNull())
        mimeType = MIMETypeRegistry::getMIMETypeForPath(urlFilename);
    m_response.setMimeType(mimeType);

    // Set encoding from Content-Type header.
    m_response.setTextEncodingName(extractCharsetFromMediaType(m_contentType));

    // Set content length from header.
    String contentLength = m_response.httpHeaderField("Content-Length");
    if (!contentLength.isNull())
        m_response.setExpectedContentLength(contentLength.toInt64());

    // Set suggested filename for downloads from the Content-Disposition header; if this fails,
    // fill it in from the url and sniffed mime type;Skip this for data and about URLs,
    // because they have no Content-Disposition header and the format is wrong to be a filename.
    if (!m_isData && !m_isAbout) {
        String suggestedFilename = filenameFromHTTPContentDisposition(m_contentDisposition);
        if (suggestedFilename.isEmpty()) {
            // Check and see if an extension already exists.
            String mimeExtension = MIMETypeRegistry::getPreferredExtensionForMIMEType(mimeType);
            if (urlFilename.isEmpty()) {
                if (mimeExtension.isEmpty()) // No extension found for the mimeType.
                    suggestedFilename = String("Untitled");
                else
                    suggestedFilename = String("Untitled") + "." + mimeExtension;
            } else {
                if (urlFilename.reverseFind('.') == notFound && !mimeExtension.isEmpty())
                   suggestedFilename = urlFilename + '.' + mimeExtension;
                else
                   suggestedFilename = urlFilename;
            }
        }
        m_response.setSuggestedFilename(suggestedFilename);
    }

    // Make sure local files aren't cached, since this just duplicates them.
    if (m_isFile || m_isData || m_isAbout)
        m_response.setHTTPHeaderField("Cache-Control", "no-cache");

    if (isClientAvailable()) {
        RecursionGuard guard(m_callingClient);
        m_handle->client()->didReceiveResponse(m_handle.get(), m_response);
    }
}

void NetworkJob::sendMultipartResponseIfNeeded()
{
    if (m_multipartResponse && isClientAvailable()) {
        m_handle->client()->didReceiveResponse(m_handle.get(), *m_multipartResponse);
        m_multipartResponse = nullptr;
    }
}

void NetworkJob::parseData()
{
    Vector<char> result;

    String contentType("text/plain;charset=US-ASCII");

    String data(m_response.url().string().substring(5));
    Vector<String> hparts;
    bool isBase64 = false;

    size_t index = data.find(',');
    if (index != notFound && index > 0) {
        contentType = data.left(index).lower();
        data = data.substring(index + 1);

        contentType.split(';', hparts);
        Vector<String>::iterator i;
        for (i = hparts.begin(); i != hparts.end(); ++i) {
            if (i->stripWhiteSpace().lower() == "base64") {
                isBase64 = true;
                String value = *i;
                do {
                    if (*i == value) {
                        int position = i - hparts.begin();
                        hparts.remove(position);
                        i = hparts.begin() + position;
                    } else
                        ++i;
                } while (i != hparts.end());
                break;
            }
        }
        contentType = String();
        for (i = hparts.begin(); i != hparts.end(); ++i) {
            if (i > hparts.begin())
                contentType += ",";

            contentType += *i;
        }
    } else if (!index)
        data = data.substring(1); // Broken header.

    {
        CString latin = data.latin1();
        escapeDecode(latin.data(), latin.length(), result);
    }

    if (isBase64) {
        String s(result.data(), result.size());
        CString latin = s.removeCharacters(isSpaceOrNewline).latin1();
        result.clear();
        result.append(latin.data(), latin.length());
        Vector<char> bytesOut;
        if (base64Decode(result, bytesOut))
            result.swap(bytesOut);
        else
            result.clear();
    }

    notifyStatusReceived(result.isEmpty() ? 404 : 200, 0);
    notifyStringHeaderReceived("Content-Type", contentType);
    notifyStringHeaderReceived("Content-Length", String::number(result.size()));
    notifyDataReceivedPlain(result.data(), result.size());
    notifyClose(BlackBerry::Platform::FilterStream::StatusSuccess);
}

bool NetworkJob::handleAuthHeader(const ProtectionSpaceServerType space, const String& header)
{
    if (!m_handle)
        return false;

    if (!m_handle->getInternal()->m_currentWebChallenge.isNull())
        return false;

    if (header.isEmpty())
        return false;

    if (equalIgnoringCase(header, "ntlm"))
        sendRequestWithCredentials(space, ProtectionSpaceAuthenticationSchemeNTLM, "NTLM");

    // Extract the auth scheme and realm from the header.
    size_t spacePos = header.find(' ');
    if (spacePos == notFound) {
        LOG(Network, "%s-Authenticate field '%s' badly formatted: missing scheme.", space == ProtectionSpaceServerHTTP ? "WWW" : "Proxy", header.utf8().data());
        return false;
    }

    String scheme = header.left(spacePos);

    ProtectionSpaceAuthenticationScheme protectionSpaceScheme = ProtectionSpaceAuthenticationSchemeDefault;
    if (equalIgnoringCase(scheme, "basic"))
        protectionSpaceScheme = ProtectionSpaceAuthenticationSchemeHTTPBasic;
    else if (equalIgnoringCase(scheme, "digest"))
        protectionSpaceScheme = ProtectionSpaceAuthenticationSchemeHTTPDigest;
    else {
        notImplemented();
        return false;
    }

    size_t realmPos = header.findIgnoringCase("realm=", spacePos);
    if (realmPos == notFound) {
        LOG(Network, "%s-Authenticate field '%s' badly formatted: missing realm.", space == ProtectionSpaceServerHTTP ? "WWW" : "Proxy", header.utf8().data());
        return false;
    }
    size_t beginPos = realmPos + 6;
    String realm  = header.right(header.length() - beginPos);
    if (realm.startsWith("\"")) {
        beginPos += 1;
        size_t endPos = header.find("\"", beginPos);
        if (endPos == notFound) {
            LOG(Network, "%s-Authenticate field '%s' badly formatted: invalid realm.", space == ProtectionSpaceServerHTTP ? "WWW" : "Proxy", header.utf8().data());
            return false;
        }
        realm = header.substring(beginPos, endPos - beginPos);
    }

    // Get the user's credentials and resend the request.
    sendRequestWithCredentials(space, protectionSpaceScheme, realm);

    return true;
}

bool NetworkJob::handleFTPHeader(const String& header)
{
    size_t spacePos = header.find(' ');
    if (spacePos == notFound)
        return false;
    String statusCode = header.left(spacePos);
    switch (statusCode.toInt()) {
    case 213:
        m_isFTPDir = false;
        break;
    case 530:
        purgeCredentials();
        sendRequestWithCredentials(ProtectionSpaceServerFTP, ProtectionSpaceAuthenticationSchemeDefault, "ftp");
        break;
    case 230:
        storeCredentials();
        break;
    case 550:
        // The user might have entered an URL which point to a directory but forgot type '/',
        // e.g., ftp://ftp.trolltech.com/qt/source where 'source' is a directory. We need to
        // added '/' and try again.
        if (m_handle && !m_handle->firstRequest().url().path().endsWith("/"))
            m_needsRetryAsFTPDirectory = true;
        break;
    }

    return true;
}

bool NetworkJob::sendRequestWithCredentials(ProtectionSpaceServerType type, ProtectionSpaceAuthenticationScheme scheme, const String& realm)
{
    ASSERT(m_handle);
    if (!m_handle)
        return false;

    KURL newURL = m_response.url();
    if (!newURL.isValid())
        return false;

    int port = 0;
    if (type == ProtectionSpaceProxyHTTP) {
        std::stringstream toPort(BlackBerry::Platform::Client::get()->getProxyPort());
        toPort >> port;
    } else
        port = m_response.url().port();

    ProtectionSpace protectionSpace((type == ProtectionSpaceProxyHTTP) ? BlackBerry::Platform::Client::get()->getProxyAddress().c_str() : m_response.url().host()
            , port, type, realm, scheme);

    // We've got the scheme and realm. Now we need a username and password.
    // First search the CredentialStorage.
    Credential credential = CredentialStorage::get(protectionSpace);
    if (!credential.isEmpty()) {
        m_handle->getInternal()->m_currentWebChallenge = AuthenticationChallenge(protectionSpace, credential, 0, m_response, ResourceError());
        m_handle->getInternal()->m_currentWebChallenge.setStored(true);
    } else {
        // CredentialStore is empty. Ask the user via dialog.
        String username;
        String password;

        if (!m_frame || !m_frame->loader() || !m_frame->loader()->client())
            return false;

        // Before asking the user for credentials, we check if the URL contains that.
        if (!m_handle->getInternal()->m_user.isEmpty() && !m_handle->getInternal()->m_pass.isEmpty()) {
            username = m_handle->getInternal()->m_user.utf8().data();
            password = m_handle->getInternal()->m_pass.utf8().data();

            // Prevent them from been used again if they are wrong.
            // If they are correct, they will the put into CredentialStorage.
            m_handle->getInternal()->m_user = "";
            m_handle->getInternal()->m_pass = "";
        } else
            m_frame->loader()->client()->authenticationChallenge(realm, username, password);

        if (username.isEmpty() && password.isEmpty())
            return false;

        credential = Credential(username, password, CredentialPersistenceForSession);

        m_handle->getInternal()->m_currentWebChallenge = AuthenticationChallenge(protectionSpace, credential, 0, m_response, ResourceError());
    }

    // FIXME: Resend the resource request. Cloned from handleRedirect(). Not sure
    // if we need everything that follows...
    ResourceRequest newRequest = m_handle->firstRequest();
    newRequest.setURL(newURL);
    newRequest.setMustHandleInternally(true);
    return startNewJobWithRequest(newRequest);
}

void NetworkJob::storeCredentials()
{
    if (!m_handle)
        return;

    AuthenticationChallenge& challenge = m_handle->getInternal()->m_currentWebChallenge;
    if (challenge.isNull())
        return;

    if (challenge.isStored())
        return;

    CredentialStorage::set(challenge.proposedCredential(), challenge.protectionSpace(), m_response.url());
    challenge.setStored(true);
}

void NetworkJob::purgeCredentials()
{
    if (!m_handle)
        return;

    AuthenticationChallenge& challenge = m_handle->getInternal()->m_currentWebChallenge;
    if (challenge.isNull())
        return;

    CredentialStorage::remove(challenge.protectionSpace());
    challenge.setStored(false);
}

bool NetworkJob::shouldSendClientData() const
{
    return (!isRedirect(m_extendedStatusCode) || !m_response.httpHeaderFields().contains("Location"))
           && !m_needsRetryAsFTPDirectory;
}

void NetworkJob::fireDeleteJobTimer(Timer<NetworkJob>*)
{
    NetworkManager::instance()->deleteJob(this);
}

void NetworkJob::handleAbout()
{
    // First 6 chars are "about:".
    String aboutWhat(m_response.url().string().substring(6));

    String result;

    bool handled = false;
    if (aboutWhat.isEmpty() || equalIgnoringCase(aboutWhat, "blank")) {
        handled = true;
    } else if (equalIgnoringCase(aboutWhat, "credits")) {
        result.append(String("<html><head><title>Open Source Credits</title> <style> .about {padding:14px;} </style> <meta name=\"viewport\" content=\"width=device-width, user-scalable=no\"></head><body>"));
        result.append(String(BlackBerry::Platform::WEBKITCREDITS));
        result.append(String("</body></html>"));
        handled = true;
#if !defined(PUBLIC_BUILD) || !PUBLIC_BUILD
    } else if (aboutWhat.startsWith("cache?query=", false)) {
        BlackBerry::Platform::Client* client = BlackBerry::Platform::Client::get();
        ASSERT(client);
        std::string key(aboutWhat.substring(12, aboutWhat.length() - 12).utf8().data()); // 12 is length of "cache?query=".
        result.append(String("<html><head><title>BlackBerry Browser Disk Cache</title></head><body>"));
        result.append(String(key.data()));
        result.append(String("<hr>"));
        result.append(String(client->generateHtmlFragmentForCacheHeaders(key).data()));
        result.append(String("</body></html>"));
        handled = true;
    } else if (equalIgnoringCase(aboutWhat, "cache")) {
        BlackBerry::Platform::Client* client = BlackBerry::Platform::Client::get();
        ASSERT(client);
        result.append(String("<html><head><title>BlackBerry Browser Disk Cache</title></head><body>"));
        result.append(String(client->generateHtmlFragmentForCacheKeys().data()));
        result.append(String("</body></html>"));
        handled = true;
    } else if (equalIgnoringCase(aboutWhat, "cache/disable")) {
        BlackBerry::Platform::Client* client = BlackBerry::Platform::Client::get();
        ASSERT(client);
        client->setDiskCacheEnabled(false);
        result.append(String("<html><head><title>BlackBerry Browser Disk Cache</title></head><body>Http disk cache is disabled.</body></html>"));
        handled = true;
    } else if (equalIgnoringCase(aboutWhat, "cache/enable")) {
        BlackBerry::Platform::Client* client = BlackBerry::Platform::Client::get();
        ASSERT(client);
        client->setDiskCacheEnabled(true);
        result.append(String("<html><head><title>BlackBerry Browser Disk Cache</title></head><body>Http disk cache is enabled.</body></html>"));
        handled = true;
    } else if (equalIgnoringCase(aboutWhat, "version")) {
        result.append(String("<html><meta name=\"viewport\" content=\"width=device-width, user-scalable=no\"></head><body>"));
        result.append(String(BlackBerry::Platform::BUILDTIME));
        result.append(String("</body></html>"));
        handled = true;
    } else if (BlackBerry::Platform::debugSetting() > 0 && equalIgnoringCase(aboutWhat, "config")) {
        result = configPage();
        handled = true;
    } else if (BlackBerry::Platform::debugSetting() > 0 && equalIgnoringCase(aboutWhat, "build")) {
        result.append(String("<html><head><title>BlackBerry Browser Build Information</title></head><body><table>"));
        result.append(String("<tr><td>Build Computer:  </td><td>"));
        result.append(String(BlackBerry::Platform::BUILDCOMPUTER));
        result.append(String("</td></tr>"));
        result.append(String("<tr><td>Build User:  </td><td>"));
        result.append(String(BlackBerry::Platform::BUILDUSER));
        result.append(String("</td></tr>"));
        result.append(String("<tr><td>Build Time:  </td><td>"));
        result.append(String(BlackBerry::Platform::BUILDTIME));
        result.append(String("</td></tr><tr><td></td><td></td></tr>"));
        result.append(String(BlackBerry::Platform::BUILDINFO_WEBKIT));
        result.append(String(BlackBerry::Platform::BUILDINFO_PLATFORM));
        result.append(String(BlackBerry::Platform::BUILDINFO_LIBWEBVIEW));
        result.append(String("</table></body></html>"));
        handled = true;
    } else if (equalIgnoringCase(aboutWhat, "memory")) {
        result = memoryPage();
        handled = true;
#endif
    }

    if (handled) {
        CString resultString = result.utf8();
        notifyStatusReceived(404, 0);
        notifyStringHeaderReceived("Content-Length", String::number(resultString.length()));
        notifyStringHeaderReceived("Content-Type", "text/html");
        notifyDataReceivedPlain(resultString.data(), resultString.length());
        notifyClose(BlackBerry::Platform::FilterStream::StatusSuccess);
    } else {
        // If we can not handle it, we take it as an error of invalid URL.
        notifyStatusReceived(BlackBerry::Platform::FilterStream::StatusErrorInvalidUrl, 0);
        notifyClose(BlackBerry::Platform::FilterStream::StatusErrorInvalidUrl);
    }
}

} // namespace WebCore
