/*
 * Copyright (C) 2004, 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2005, 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
 * All rights reserved.
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
#include "ResourceHandle.h"

#if USE(CURL)

#include "CachedResourceLoader.h"
#include "CredentialStorage.h"
#include "CurlCacheManager.h"
#include "CurlContext.h"
#include "FileSystem.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "NetworkingContext.h"
#include "NotImplemented.h"
#include "ResourceHandleInternal.h"
#include "ResourceHandleManager.h"
#include "SSLHandle.h"
#include "SynchronousLoaderClient.h"
#include <wtf/text/Base64.h>

namespace WebCore {

ResourceHandleInternal::~ResourceHandleInternal()
{

}

ResourceHandle::~ResourceHandle()
{
    cancel();
}

bool ResourceHandle::start()
{
    // The frame could be null if the ResourceHandle is not associated to any
    // Frame, e.g. if we are downloading a file.
    // If the frame is not null but the page is null this must be an attempted
    // load from an unload handler, so let's just block it.
    // If both the frame and the page are not null the context is valid.
    if (d->m_context && !d->m_context->isValid())
        return false;

    ResourceHandleManager::sharedInstance()->add(this);
    return true;
}

void ResourceHandle::cancel()
{
    ResourceHandleManager::sharedInstance()->cancel(this);
}

#if OS(WINDOWS)

void ResourceHandle::setHostAllowsAnyHTTPSCertificate(const String& host)
{
    allowsAnyHTTPSCertificateHosts(host);
}

void ResourceHandle::setClientCertificateInfo(const String& host, const String& certificate, const String& key)
{
    if (fileExists(certificate))
        addAllowedClientCertificate(host, certificate, key);
    else
        LOG(Network, "Invalid client certificate file: %s!\n", certificate.latin1().data());
}

#endif

#if OS(WINDOWS) && USE(CF)

void ResourceHandle::setClientCertificate(const String&, CFDataRef)
{
}

#endif

void ResourceHandle::platformSetDefersLoading(bool defers)
{
    if (!d->m_curlHandle.handle())
        return;

    if (defers) {
        CURLcode error = d->m_curlHandle.pause(CURLPAUSE_ALL);
        // If we could not defer the handle, so don't do it.
        if (error != CURLE_OK)
            return;
    } else {
        CURLcode error = d->m_curlHandle.pause(CURLPAUSE_CONT);
        if (error != CURLE_OK)
            // Restarting the handle has failed so just cancel it.
            cancel();
    }
}

bool ResourceHandle::shouldUseCredentialStorage()
{
    return (!client() || client()->shouldUseCredentialStorage(this)) && firstRequest().url().protocolIsInHTTPFamily();
}

void ResourceHandle::platformLoadResourceSynchronously(NetworkingContext* context, const ResourceRequest& request, StoredCredentials, ResourceError& error, ResourceResponse& response, Vector<char>& data)
{
    SynchronousLoaderClient client;
    RefPtr<ResourceHandle> handle = adoptRef(new ResourceHandle(context, request, &client, false, false));

    handle.get()->dispatchSynchronousJob();

    error = client.error();
    data.swap(client.mutableData());
    response = client.response();
}

void ResourceHandle::didReceiveAuthenticationChallenge(const AuthenticationChallenge& challenge)
{
    String partition = firstRequest().cachePartition();

    if (!d->m_user.isNull() && !d->m_pass.isNull()) {
        Credential credential(d->m_user, d->m_pass, CredentialPersistenceNone);

        URL urlToStore;
        if (challenge.failureResponse().httpStatusCode() == 401)
            urlToStore = challenge.failureResponse().url();
        CredentialStorage::defaultCredentialStorage().set(partition, credential, challenge.protectionSpace(), urlToStore);
        
        d->m_curlHandle.setHttpAuthUserPass(credential.user(), credential.password());

        d->m_user = String();
        d->m_pass = String();
        // FIXME: Per the specification, the user shouldn't be asked for credentials if there were incorrect ones provided explicitly.
        return;
    }

    if (shouldUseCredentialStorage()) {
        if (!d->m_initialCredential.isEmpty() || challenge.previousFailureCount()) {
            // The stored credential wasn't accepted, stop using it.
            // There is a race condition here, since a different credential might have already been stored by another ResourceHandle,
            // but the observable effect should be very minor, if any.
            CredentialStorage::defaultCredentialStorage().remove(partition, challenge.protectionSpace());
        }

        if (!challenge.previousFailureCount()) {
            Credential credential = CredentialStorage::defaultCredentialStorage().get(partition, challenge.protectionSpace());
            if (!credential.isEmpty() && credential != d->m_initialCredential) {
                ASSERT(credential.persistence() == CredentialPersistenceNone);
                if (challenge.failureResponse().httpStatusCode() == 401) {
                    // Store the credential back, possibly adding it as a default for this directory.
                    CredentialStorage::defaultCredentialStorage().set(partition, credential, challenge.protectionSpace(), challenge.failureResponse().url());
                }

                d->m_curlHandle.setHttpAuthUserPass(credential.user(), credential.password());
                return;
            }
        }
    }

    d->m_currentWebChallenge = challenge;
    
    if (client())
        client()->didReceiveAuthenticationChallenge(this, d->m_currentWebChallenge);
}

void ResourceHandle::receivedCredential(const AuthenticationChallenge& challenge, const Credential& credential)
{
    if (challenge != d->m_currentWebChallenge)
        return;

    if (credential.isEmpty()) {
        receivedRequestToContinueWithoutCredential(challenge);
        return;
    }

    String partition = firstRequest().cachePartition();

    if (shouldUseCredentialStorage()) {
        if (challenge.failureResponse().httpStatusCode() == 401) {
            URL urlToStore = challenge.failureResponse().url();
            CredentialStorage::defaultCredentialStorage().set(partition, credential, challenge.protectionSpace(), urlToStore);
        }
    }

    d->m_curlHandle.setHttpAuthUserPass(credential.user(), credential.password());
    clearAuthentication();
}

void ResourceHandle::receivedRequestToContinueWithoutCredential(const AuthenticationChallenge& challenge)
{
    if (challenge != d->m_currentWebChallenge)
        return;

    d->m_curlHandle.setHttpAuthUserPass("", "");
    clearAuthentication();
}

void ResourceHandle::receivedCancellation(const AuthenticationChallenge& challenge)
{
    if (challenge != d->m_currentWebChallenge)
        return;

    if (client())
        client()->receivedCancellation(this, challenge);
}

void ResourceHandle::receivedRequestToPerformDefaultHandling(const AuthenticationChallenge&)
{
    ASSERT_NOT_REACHED();
}

void ResourceHandle::receivedChallengeRejection(const AuthenticationChallenge&)
{
    ASSERT_NOT_REACHED();
}

const char* const errorDomainCurl = "CurlErrorDomain";

#if ENABLE(WEB_TIMING)
static void calculateWebTimingInformations(ResourceHandleInternal* d)
{
    double preTransferTime = 0;
    double dnslookupTime = 0;
    double connectTime = 0;
    double appConnectTime = 0;

    d->m_curlHandle.getTimes(preTransferTime, dnslookupTime, connectTime, appConnectTime);

    d->m_response.deprecatedNetworkLoadMetrics().domainLookupStart = Seconds(0);
    d->m_response.deprecatedNetworkLoadMetrics().domainLookupEnd = Seconds(dnslookupTime);

    d->m_response.deprecatedNetworkLoadMetrics().connectStart = Seconds(dnslookupTime);
    d->m_response.deprecatedNetworkLoadMetrics().connectEnd = Seconds(connectTime);

    d->m_response.deprecatedNetworkLoadMetrics().requestStart = Seconds(connectTime);
    d->m_response.deprecatedNetworkLoadMetrics().responseStart = Seconds(preTransferTime);

    if (appConnectTime)
        d->m_response.deprecatedNetworkLoadMetrics().secureConnectionStart = Seconds(connectTime);
}
#endif

static void handleLocalReceiveResponse(ResourceHandle* job, ResourceHandleInternal* d)
{
    // since the code in headerCallback will not have run for local files
    // the code to set the URL and fire didReceiveResponse is never run,
    // which means the ResourceLoader's response does not contain the URL.
    // Run the code here for local files to resolve the issue.
    // TODO: See if there is a better approach for handling this.
    URL url = d->m_curlHandle.getEffectiveURL();
    ASSERT(url.isValid());
    d->m_response.setURL(url);
    if (d->client())
        d->client()->didReceiveResponse(job, ResourceResponse(d->m_response));
    d->m_response.setResponseFired(true);
}

// called with data after all headers have been processed via headerCallback
static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* data)
{
    ResourceHandle* job = static_cast<ResourceHandle*>(data);
    ResourceHandleInternal* d = job->getInternal();
    if (d->m_cancelled)
        return 0;

    // We should never be called when deferred loading is activated.
    ASSERT(!d->m_defersLoading);

    size_t totalSize = size * nmemb;

    // this shouldn't be necessary but apparently is. CURL writes the data
    // of html page even if it is a redirect that was handled internally
    // can be observed e.g. on gmail.com
    long httpCode = 0;
    CURLcode errCd = d->m_curlHandle.getResponseCode(httpCode);
    if (CURLE_OK == errCd && httpCode >= 300 && httpCode < 400)
        return totalSize;

    if (!d->m_response.responseFired()) {
        handleLocalReceiveResponse(job, d);
        if (d->m_cancelled)
            return 0;
    }

    if (d->m_multipartHandle)
        d->m_multipartHandle->contentReceived(static_cast<const char*>(ptr), totalSize);
    else if (d->client()) {
        d->client()->didReceiveData(job, ptr, totalSize, 0);
        CurlCacheManager::getInstance().didReceiveData(*job, ptr, totalSize);
    }

    return totalSize;
}

inline static bool isHttpInfo(int statusCode)
{
    return 100 <= statusCode && statusCode < 200;
}

inline static bool isHttpRedirect(int statusCode)
{
    return 300 <= statusCode && statusCode < 400 && statusCode != 304;
}

inline static bool isHttpAuthentication(int statusCode)
{
    return statusCode == 401;
}

inline static bool isHttpNotModified(int statusCode)
{
    return statusCode == 304;
}

static bool isAppendableHeader(const String &key)
{
    static const char* appendableHeaders[] = {
        "access-control-allow-headers",
        "access-control-allow-methods",
        "access-control-allow-origin",
        "access-control-expose-headers",
        "allow",
        "cache-control",
        "connection",
        "content-encoding",
        "content-language",
        "if-match",
        "if-none-match",
        "keep-alive",
        "pragma",
        "proxy-authenticate",
        "public",
        "server",
        "set-cookie",
        "te",
        "trailer",
        "transfer-encoding",
        "upgrade",
        "user-agent",
        "vary",
        "via",
        "warning",
        "www-authenticate"
    };

    // Custom headers start with 'X-', and need no further checking.
    if (key.startsWith("x-", /* caseSensitive */ false))
        return true;

    for (auto& header : appendableHeaders) {
        if (equalIgnoringASCIICase(key, header))
            return true;
    }

    return false;
}

static void removeLeadingAndTrailingQuotes(String& value)
{
    unsigned length = value.length();
    if (value.startsWith('"') && value.endsWith('"') && length > 1)
        value = value.substring(1, length - 2);
}

static bool getProtectionSpace(ResourceHandle* job, const ResourceResponse& response, ProtectionSpace& protectionSpace)
{
    ResourceHandleInternal* d = job->getInternal();

    CURLcode err;

    long port = 0;
    err = d->m_curlHandle.getPrimaryPort(port);
    if (err != CURLE_OK)
        return false;

    long availableAuth = CURLAUTH_NONE;
    err = d->m_curlHandle.getHttpAuthAvail(availableAuth);
    if (err != CURLE_OK)
        return false;

    URL url = d->m_curlHandle.getEffectiveURL();
    if (!url.isValid())
        return false;

    String host = url.host();
    StringView protocol = url.protocol();

    String realm;

    const String authHeader = response.httpHeaderField(HTTPHeaderName::Authorization);
    const String realmString = "realm=";
    int realmPos = authHeader.find(realmString);
    if (realmPos > 0) {
        realm = authHeader.substring(realmPos + realmString.length());
        realm = realm.left(realm.find(','));
        removeLeadingAndTrailingQuotes(realm);
    }

    ProtectionSpaceServerType serverType = ProtectionSpaceServerHTTP;
    if (protocol == "https")
        serverType = ProtectionSpaceServerHTTPS;

    ProtectionSpaceAuthenticationScheme authScheme = ProtectionSpaceAuthenticationSchemeUnknown;

    if (availableAuth & CURLAUTH_BASIC)
        authScheme = ProtectionSpaceAuthenticationSchemeHTTPBasic;
    if (availableAuth & CURLAUTH_DIGEST)
        authScheme = ProtectionSpaceAuthenticationSchemeHTTPDigest;
    if (availableAuth & CURLAUTH_GSSNEGOTIATE)
        authScheme = ProtectionSpaceAuthenticationSchemeNegotiate;
    if (availableAuth & CURLAUTH_NTLM)
        authScheme = ProtectionSpaceAuthenticationSchemeNTLM;

    protectionSpace = ProtectionSpace(host, port, serverType, realm, authScheme);

    return true;
}

/*
* This is being called for each HTTP header in the response. This includes '\r\n'
* for the last line of the header.
*
* We will add each HTTP Header to the ResourceResponse and on the termination
* of the header (\r\n) we will parse Content-Type and Content-Disposition and
* update the ResourceResponse and then send it away.
*
*/
static size_t headerCallback(char* ptr, size_t size, size_t nmemb, void* data)
{
    ResourceHandle* job = static_cast<ResourceHandle*>(data);
    ResourceHandleInternal* d = job->getInternal();
    if (d->m_cancelled)
        return 0;

    // We should never be called when deferred loading is activated.
    ASSERT(!d->m_defersLoading);

    size_t totalSize = size * nmemb;
    ResourceHandleClient* client = d->client();

    String header(static_cast<const char*>(ptr), totalSize);

    /*
    * a) We can finish and send the ResourceResponse
    * b) We will add the current header to the HTTPHeaderMap of the ResourceResponse
    *
    * The HTTP standard requires to use \r\n but for compatibility it recommends to
    * accept also \n.
    */
    if (header == String("\r\n") || header == String("\n")) {
        long httpCode = 0;
        d->m_curlHandle.getResponseCode(httpCode);

        if (!httpCode) {
            // Comes here when receiving 200 Connection Established. Just return.
            return totalSize;
        }
        if (isHttpInfo(httpCode)) {
            // Just return when receiving http info, e.g. HTTP/1.1 100 Continue.
            // If not, the request might be cancelled, because the MIME type will be empty for this response.
            return totalSize;
        }

        long long contentLength = 0;
        d->m_curlHandle.getContentLenghtDownload(contentLength);
        d->m_response.setExpectedContentLength(contentLength);

        d->m_response.setURL(d->m_curlHandle.getEffectiveURL());

        d->m_response.setHTTPStatusCode(httpCode);
        d->m_response.setMimeType(extractMIMETypeFromMediaType(d->m_response.httpHeaderField(HTTPHeaderName::ContentType)).convertToASCIILowercase());
        d->m_response.setTextEncodingName(extractCharsetFromMediaType(d->m_response.httpHeaderField(HTTPHeaderName::ContentType)));

        if (d->m_response.isMultipart()) {
            String boundary;
            bool parsed = MultipartHandle::extractBoundary(d->m_response.httpHeaderField(HTTPHeaderName::ContentType), boundary);
            if (parsed)
                d->m_multipartHandle = std::make_unique<MultipartHandle>(job, boundary);
        }

        // HTTP redirection
        if (isHttpRedirect(httpCode)) {
            String location = d->m_response.httpHeaderField(HTTPHeaderName::Location);
            if (!location.isEmpty()) {
                URL newURL = URL(job->firstRequest().url(), location);

                ResourceRequest redirectedRequest = job->firstRequest();
                redirectedRequest.setURL(newURL);
                ResourceResponse response = d->m_response;
                if (client)
                    client->willSendRequest(job, WTFMove(redirectedRequest), WTFMove(response));

                d->m_firstRequest.setURL(newURL);

                return totalSize;
            }
        } else if (isHttpAuthentication(httpCode)) {
            ProtectionSpace protectionSpace;
            if (getProtectionSpace(job, d->m_response, protectionSpace)) {
                Credential credential;
                AuthenticationChallenge challenge(protectionSpace, credential, d->m_authFailureCount, d->m_response, ResourceError());
                challenge.setAuthenticationClient(job);
                job->didReceiveAuthenticationChallenge(challenge);
                d->m_authFailureCount++;
                return totalSize;
            }
        }

        if (client) {
            if (isHttpNotModified(httpCode)) {
                const String& url = job->firstRequest().url().string();
                if (CurlCacheManager::getInstance().getCachedResponse(url, d->m_response)) {
                    if (d->m_addedCacheValidationHeaders) {
                        d->m_response.setHTTPStatusCode(200);
                        d->m_response.setHTTPStatusText("OK");
                    }
                }
            }
            client->didReceiveResponse(job, ResourceResponse(d->m_response));
            CurlCacheManager::getInstance().didReceiveResponse(*job, d->m_response);
        }
        d->m_response.setResponseFired(true);

    } else {
        int splitPos = header.find(":");
        if (splitPos != -1) {
            String key = header.left(splitPos).stripWhiteSpace();
            String value = header.substring(splitPos + 1).stripWhiteSpace();

            if (isAppendableHeader(key))
                d->m_response.addHTTPHeaderField(key, value);
            else
                d->m_response.setHTTPHeaderField(key, value);
        } else if (header.startsWith("HTTP", false)) {
            // This is the first line of the response.
            // Extract the http status text from this.
            //
            // If the FOLLOWLOCATION option is enabled for the curl handle then
            // curl will follow the redirections internally. Thus this header callback
            // will be called more than one time with the line starting "HTTP" for one job.
            long httpCode = 0;
            d->m_curlHandle.getResponseCode(httpCode);

            String httpCodeString = String::number(httpCode);
            int statusCodePos = header.find(httpCodeString);

            if (statusCodePos != -1) {
                // The status text is after the status code.
                String status = header.substring(statusCodePos + httpCodeString.length());
                d->m_response.setHTTPStatusText(status.stripWhiteSpace());
            }

        }
    }

    return totalSize;
}

/* This is called to obtain HTTP POST or PUT data.
Iterate through FormData elements and upload files.
Carefully respect the given buffer size and fill the rest of the data at the next calls.
*/
size_t readCallback(char* ptr, size_t size, size_t nmemb, void* data)
{
    ResourceHandle* job = static_cast<ResourceHandle*>(data);
    ResourceHandleInternal* d = job->getInternal();

    if (d->m_cancelled)
        return 0;

    // We should never be called when deferred loading is activated.
    ASSERT(!d->m_defersLoading);

    if (!size || !nmemb)
        return 0;

    if (!d->m_formDataStream.hasMoreElements())
        return 0;

    size_t sent = d->m_formDataStream.read(ptr, size, nmemb);

    // Something went wrong so cancel the job.
    if (!sent)
        job->cancel();

    return sent;
}

static inline size_t getFormElementsCount(ResourceHandle* job)
{
    RefPtr<FormData> formData = job->firstRequest().httpBody();

    if (!formData)
        return 0;

    // Resolve the blob elements so the formData can correctly report it's size.
    formData = formData->resolveBlobReferences();
    size_t size = formData->elements().size();
    job->firstRequest().setHTTPBody(WTFMove(formData));

    return size;
}

static void setupFormData(ResourceHandle* job, bool isPostRequest)
{
    ResourceHandleInternal* d = job->getInternal();
    Vector<FormDataElement> elements = job->firstRequest().httpBody()->elements();
    size_t numElements = elements.size();

    static const long long maxCurlOffT = d->m_curlHandle.maxCurlOffT();

    // Obtain the total size of the form data
    curl_off_t size = 0;
    bool chunkedTransfer = false;
    for (size_t i = 0; i < numElements; i++) {
        FormDataElement element = elements[i];
        if (element.m_type == FormDataElement::Type::EncodedFile) {
            long long fileSizeResult;
            if (getFileSize(element.m_filename, fileSizeResult)) {
                if (fileSizeResult > maxCurlOffT) {
                    // File size is too big for specifying it to cURL
                    chunkedTransfer = true;
                    break;
                }
                size += fileSizeResult;
            } else {
                chunkedTransfer = true;
                break;
            }
        } else
            size += elements[i].m_data.size();
    }

    // cURL guesses that we want chunked encoding as long as we specify the header
    if (chunkedTransfer)
        d->m_curlHandle.appendRequestHeader("Transfer-Encoding: chunked");
    else {
        if (isPostRequest)
            d->m_curlHandle.setPostFieldLarge(size);
        else
            d->m_curlHandle.setInFileSizeLarge(size);
    }

    d->m_curlHandle.setReadCallbackFunction(readCallback, job);
}

void ResourceHandle::setupPUT()
{
    ResourceHandleInternal* d = getInternal();
    d->m_curlHandle.enableHttpPutRequest();

    // Disable the Expect: 100 continue header
    d->m_curlHandle.appendRequestHeader("Expect:");

    size_t numElements = getFormElementsCount(this);
    if (!numElements)
        return;

    setupFormData(this, false);
}

void ResourceHandle::setupPOST()
{
    ResourceHandleInternal* d = getInternal();
    d->m_curlHandle.enableHttpPostRequest();

    size_t numElements = getFormElementsCount(this);
    if (!numElements)
        return;

    // Do not stream for simple POST data
    if (numElements == 1) {
        firstRequest().httpBody()->flatten(d->m_postBytes);
        if (d->m_postBytes.size())
            d->m_curlHandle.setPostFields(d->m_postBytes.data(), d->m_postBytes.size());
        return;
    }

    setupFormData(this, true);
}

void ResourceHandle::handleDataURL()
{
    ASSERT(firstRequest().url().protocolIsData());
    String url = firstRequest().url().string();

    ASSERT(client());

    int index = url.find(',');
    if (index == -1) {
        client()->cannotShowURL(this);
        return;
    }

    String mediaType = url.substring(5, index - 5);
    String data = url.substring(index + 1);

    bool base64 = mediaType.endsWith(";base64", false);
    if (base64)
        mediaType = mediaType.left(mediaType.length() - 7);

    if (mediaType.isEmpty())
        mediaType = "text/plain";

    String mimeType = extractMIMETypeFromMediaType(mediaType);
    String charset = extractCharsetFromMediaType(mediaType);

    if (charset.isEmpty())
        charset = "US-ASCII";

    ResourceResponse response;
    response.setMimeType(mimeType);
    response.setTextEncodingName(charset);
    response.setURL(firstRequest().url());

    if (base64) {
        data = decodeURLEscapeSequences(data);
        client()->didReceiveResponse(this, WTFMove(response));

        // didReceiveResponse might cause the client to be deleted.
        if (client()) {
            Vector<char> out;
            if (base64Decode(data, out, Base64IgnoreSpacesAndNewLines) && out.size() > 0)
                client()->didReceiveData(this, out.data(), out.size(), 0);
        }
    } else {
        TextEncoding encoding(charset);
        data = decodeURLEscapeSequences(data, encoding);
        client()->didReceiveResponse(this, WTFMove(response));

        // didReceiveResponse might cause the client to be deleted.
        if (client()) {
            CString encodedData = encoding.encode(data, URLEncodedEntitiesForUnencodables);
            if (encodedData.length())
                client()->didReceiveData(this, encodedData.data(), encodedData.length(), 0);
        }
    }

    if (client())
        client()->didFinishLoading(this);
}

void ResourceHandle::dispatchSynchronousJob()
{
    URL kurl = firstRequest().url();

    if (kurl.protocolIsData()) {
        handleDataURL();
        return;
    }

    ResourceHandleInternal* d = getInternal();

    // If defersLoading is true and we call curl_easy_perform
    // on a paused handle, libcURL would do the transfert anyway
    // and we would assert so force defersLoading to be false.
    d->m_defersLoading = false;

    initialize();

    // curl_easy_perform blocks until the transfert is finished.
    CURLcode ret = d->m_curlHandle.perform();

    if (ret != CURLE_OK) {
        ResourceError error(ASCIILiteral(errorDomainCurl), ret, kurl, String(curl_easy_strerror(ret)));
        error.setSSLErrors(d->m_sslErrors);
        d->client()->didFail(this, error);
    } else {
        if (d->client())
            d->client()->didReceiveResponse(this, ResourceResponse(d->m_response));
    }

#if ENABLE(WEB_TIMING)
    calculateWebTimingInformations(d);
#endif
}

void ResourceHandle::applyAuthentication()
{
    ResourceRequest& request = firstRequest();
    // m_user/m_pass are credentials given manually, for instance, by the arguments passed to XMLHttpRequest.open().
    ResourceHandleInternal* d = getInternal();

    String partition = request.cachePartition();

    if (shouldUseCredentialStorage()) {
        if (d->m_user.isEmpty() && d->m_pass.isEmpty()) {
            // <rdar://problem/7174050> - For URLs that match the paths of those previously challenged for HTTP Basic authentication, 
            // try and reuse the credential preemptively, as allowed by RFC 2617.
            d->m_initialCredential = CredentialStorage::defaultCredentialStorage().get(partition, request.url());
        } else {
            // If there is already a protection space known for the URL, update stored credentials
            // before sending a request. This makes it possible to implement logout by sending an
            // XMLHttpRequest with known incorrect credentials, and aborting it immediately (so that
            // an authentication dialog doesn't pop up).
            CredentialStorage::defaultCredentialStorage().set(partition, Credential(d->m_user, d->m_pass, CredentialPersistenceNone), request.url());
        }
    }

    String user = d->m_user;
    String password = d->m_pass;

    if (!d->m_initialCredential.isEmpty()) {
        user = d->m_initialCredential.user();
        password = d->m_initialCredential.password();
        d->m_curlHandle.enableHttpAuthentication(CURLAUTH_BASIC);
    }

    // It seems we need to set CURLOPT_USERPWD even if username and password is empty.
    // Otherwise cURL will not automatically continue with a new request after a 401 response.

    // curl CURLOPT_USERPWD expects username:password
    d->m_curlHandle.setHttpAuthUserPass(user, password);
}

void ResourceHandle::initialize()
{
    CurlContext& context = CurlContext::singleton();

    URL url = firstRequest().url();

    // Remove any fragment part, otherwise curl will send it as part of the request.
    url.removeFragmentIdentifier();

    ResourceHandleInternal* d = getInternal();
    String urlString = url.string();

    if (url.isLocalFile()) {
        // Remove any query part sent to a local file.
        if (!url.query().isEmpty()) {
            // By setting the query to a null string it'll be removed.
            url.setQuery(String());
            urlString = url.string();
        }
        // Determine the MIME type based on the path.
        d->m_response.setMimeType(MIMETypeRegistry::getMIMETypeForPath(url));
    }

    if (d->m_defersLoading) {
        CURLcode error = d->m_curlHandle.pause(CURLPAUSE_ALL);
        // If we did not pause the handle, we would ASSERT in the
        // header callback. So just assert here.
        ASSERT_UNUSED(error, error == CURLE_OK);
    }

#ifndef NDEBUG
    d->m_curlHandle.enableVerboseIfUsed();
    d->m_curlHandle.enableStdErrIfUsed();
#endif

    d->m_curlHandle.setSslVerifyPeer(CurlHandle::VerifyPeerEnable);
    d->m_curlHandle.setSslVerifyHost(CurlHandle::VerifyHostStrictNameCheck);
    d->m_curlHandle.setPrivateData(this);
    d->m_curlHandle.setWriteCallbackFunction(writeCallback, this);
    d->m_curlHandle.setHeaderCallbackFunction(headerCallback, this);
    d->m_curlHandle.enableAutoReferer();
    d->m_curlHandle.enableFollowLocation();
    d->m_curlHandle.enableHttpAuthentication(CURLAUTH_ANY);
    d->m_curlHandle.enableShareHandle();
    d->m_curlHandle.enableTimeout();
    d->m_curlHandle.enableAllowedProtocols();
    setSSLClientCertificate(this);

    if (CurlContext::singleton().shouldIgnoreSSLErrors())
        d->m_curlHandle.setSslVerifyPeer(CurlHandle::VerifyPeerDisable);
    else
        setSSLVerifyOptions(this);

    d->m_curlHandle.enableCAInfoIfExists();

    d->m_curlHandle.enableAcceptEncoding();
    d->m_curlHandle.setUrl(urlString);
    d->m_curlHandle.enableCookieJarIfExists();

    d->m_curlHandle.clearRequestHeaders();

    if (firstRequest().httpHeaderFields().size() > 0) {
        HTTPHeaderMap customHeaders = firstRequest().httpHeaderFields();

        bool hasCacheHeaders = customHeaders.contains(HTTPHeaderName::IfModifiedSince) || customHeaders.contains(HTTPHeaderName::IfNoneMatch);
        if (!hasCacheHeaders && CurlCacheManager::getInstance().isCached(url)) {
            CurlCacheManager::getInstance().addCacheEntryClient(url, this);
            HTTPHeaderMap& requestHeaders = CurlCacheManager::getInstance().requestHeaders(url);

            // append additional cache information
            HTTPHeaderMap::const_iterator it = requestHeaders.begin();
            HTTPHeaderMap::const_iterator end = requestHeaders.end();
            while (it != end) {
                customHeaders.set(it->key, it->value);
                ++it;
            }
            d->m_addedCacheValidationHeaders = true;
        }

        for (auto customHeader : customHeaders)
            d->m_curlHandle.appendRequestHeader(customHeader.key, customHeader.value);
    }

    String method = firstRequest().httpMethod();
    if ("GET" == method)
        d->m_curlHandle.enableHttpGetRequest();
    else if ("POST" == method)
        setupPOST();
    else if ("PUT" == method)
        setupPUT();
    else if ("HEAD" == method)
        d->m_curlHandle.enableHttpHeadRequest();
    else {
        d->m_curlHandle.setHttpCustomRequest(method);
        setupPUT();
    }

    d->m_curlHandle.enableRequestHeaders();

    applyAuthentication();

    d->m_curlHandle.enableProxyIfExists();
}

void ResourceHandle::handleCurlMsg(CURLMsg* msg)
{
    ResourceHandleInternal* d = getInternal();

    if (CURLE_OK == msg->data.result) {
#if ENABLE(WEB_TIMING)
        calculateWebTimingInformations(d);
#endif
        if (!d->m_response.responseFired()) {
            handleLocalReceiveResponse(this, d);
            if (d->m_cancelled)
                return;
        }

        if (d->m_multipartHandle)
            d->m_multipartHandle->contentEnded();

        if (d->client()) {
            d->client()->didFinishLoading(this);
            CurlCacheManager::getInstance().didFinishLoading(*this);
        }
    } else {
        URL url = d->m_curlHandle.getEffectiveURL();
#ifndef NDEBUG
        fprintf(stderr, "Curl ERROR for url='%s', error: '%s'\n", url.string().utf8().data(), curl_easy_strerror(msg->data.result));
#endif
        if (d->client()) {
            ResourceError resourceError(ASCIILiteral(errorDomainCurl), msg->data.result, url, String(curl_easy_strerror(msg->data.result)));
            resourceError.setSSLErrors(d->m_sslErrors);
            d->client()->didFail(this, resourceError);
            CurlCacheManager::getInstance().didFail(*this);
        }
    }
}

} // namespace WebCore

#endif
