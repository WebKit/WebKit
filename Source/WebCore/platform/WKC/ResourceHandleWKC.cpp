/*
 * WKC in-process resource loading backend.
 *
 * WebKit's modern CURL path normally runs in NetworkProcess. The Wii U WKC
 * embedder runs WebCore in-process, so ResourceHandle needs a small bridge to
 * WebCore's existing CurlRequest scheduler.
 */

#include "config.h"
#include "SecurityOrigin.h"
#include "ResourceHandle.h"
#include "FormData.h"

#if USE(CURL)

#include "AuthenticationChallenge.h"
#include "CurlContext.h"
#include "CurlRequest.h"
#include "CurlRequestClient.h"
#include "CurlResponse.h"
#include "HTTPParsers.h"
#include "NetworkLoadMetrics.h"
#include "NetworkStorageSession.h"
#include "CookieRequestHeaderFieldProxy.h"
#include "SameSiteInfo.h"
#include "ResourceError.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SharedBuffer.h"

#include <curl/curl.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/URL.h>
#include <algorithm>
#include <climits>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace WebCore {
namespace {

class WKCResourceHandleCurlClient;

struct ActiveResourceTransfer {
    ActiveResourceTransfer(Ref<CurlRequest>&& request, Ref<WKCResourceHandleCurlClient>&& client)
        : request(WTF::move(request))
        , client(WTF::move(client))
    {
    }

    Ref<CurlRequest> request;
    Ref<WKCResourceHandleCurlClient> client;
};

using ActiveTransferMap = std::unordered_map<ResourceHandle*, ActiveResourceTransfer>;

ActiveTransferMap& activeTransfers()
{
    static NeverDestroyed<ActiveTransferMap> transfers;
    return transfers.get();
}

bool sameOrigin(const URL& left, const URL& right)
{
    return left.protocol() == right.protocol()
        && equalIgnoringASCIICase(left.host(), right.host())
        && left.port() == right.port();
}

bool isRedirectStatus(int status)
{
    return status == 301 || status == 302 || status == 303
        || status == 307 || status == 308;
}

// ResourceHandle's in-process WKC bridge must explicitly bridge WebCore's
// storage session to the cURL request. This keeps HTTP cookies in WebCore's
// cookie database instead of relying on cURL's separate transient cookie jar.
void applyCookiesToRequest(NetworkingContext* context, ResourceRequest& request)
{
    if (!context || !request.allowCookies() || request.hasHTTPHeaderField(HTTPHeaderName::Cookie))
        return;

    auto* session = context->storageSession();
    if (!session)
        return;

    URL firstParty = request.firstPartyForCookies();
    if (!firstParty.isValid())
        firstParty = request.url();

    CookieRequestHeaderFieldProxy proxy {
        firstParty,
        SameSiteInfo::create(request),
        request.url(),
        std::nullopt,
        std::nullopt,
        IncludeSecureCookies::Yes
    };
    auto cookieHeader = session->cookieRequestHeaderFieldValue(proxy).first;
    if (!cookieHeader.isEmpty())
        request.setHTTPHeaderField(HTTPHeaderName::Cookie, cookieHeader);
}

void storeResponseCookies(NetworkingContext* context, const ResourceRequest& request, const ResourceResponse& response)
{
    if (!context || !request.allowCookies())
        return;

    auto* session = context->storageSession();
    if (!session)
        return;

    URL firstParty = request.firstPartyForCookies();
    if (!firstParty.isValid())
        firstParty = request.url();

    // Iterate rather than asking for one header value so repeated Set-Cookie
    // fields are stored independently whenever the response representation
    // preserves them.
    for (const auto& header : response.httpHeaderFields()) {
        if (!header.keyAsHTTPHeaderName || *header.keyAsHTTPHeaderName != HTTPHeaderName::SetCookie)
            continue;
        if (!header.value.isEmpty())
            session->setCookiesFromHTTPResponse(firstParty, request.url(), header.value);
    }
}

void removeActiveTransfer(ResourceHandle* handle)
{
    auto& transfers = activeTransfers();
    auto iterator = transfers.find(handle);
    if (iterator != transfers.end())
        transfers.erase(iterator);
}

class WKCResourceHandleCurlClient final : public CurlRequestClient, public RefCounted<WKCResourceHandleCurlClient> {
public:
    WKCResourceHandleCurlClient(ResourceHandle& handle, const String& user, const String& password)
        : m_handle(&handle)
        , m_originalURL(handle.firstRequest().url())
        , m_user(user)
        , m_password(password)
    {
    }

    void ref() const final { RefCounted<WKCResourceHandleCurlClient>::ref(); }
    void deref() const final { RefCounted<WKCResourceHandleCurlClient>::deref(); }

    void curlDidSendData(CurlRequest&, unsigned long long bytesSent, unsigned long long totalBytesToBeSent) final
    {
        if (m_handle && m_handle->client())
            m_handle->client()->didSendData(m_handle.get(), bytesSent, totalBytesToBeSent);
    }

    void curlDidReceiveResponse(CurlRequest& request, CurlResponse&& curlResponse) final
    {
        RefPtr<ResourceHandle> handle = m_handle;
        if (!handle || !handle->client()) {
            request.cancel();
            removeActiveTransfer(handle.get());
            return;
        }

        ResourceResponse response(curlResponse);
        storeResponseCookies(handle->context(), request.resourceRequest(), response);
        auto status = response.httpStatusCode();
        auto location = response.httpHeaderField(HTTPHeaderName::Location);

        if (isRedirectStatus(status) && !location.isEmpty()) {
            URL redirectedURL { request.resourceRequest().url(), location };
            auto redirectSourceURL = request.resourceRequest().url();

            // This in-process browser backend deliberately follows only HTTP(S).
            // Refuse non-web or malformed destinations explicitly instead of
            // treating a redirect response as if it were the final document.
            if (!redirectedURL.isValid() || !redirectedURL.protocolIsInHTTPFamily()) {
                auto* client = handle->client();
                request.cancel();
                ResourceError error(CURLE_UNSUPPORTED_PROTOCOL, redirectSourceURL, ResourceError::Type::General);
                if (client)
                    client->didFail(handle.get(), error);
                removeActiveTransfer(handle.get());
                return;
            }

            if (handle->redirectCount() >= maximumRedirects) {
                auto* client = handle->client();
                request.cancel();
                ResourceError error(CURLE_TOO_MANY_REDIRECTS, redirectSourceURL, ResourceError::Type::General);
                if (client)
                    client->didFail(handle.get(), error);
                removeActiveTransfer(handle.get());
                return;
            }

            {
                handle->incrementRedirectCount();
                if (!sameOrigin(request.resourceRequest().url(), redirectedURL))
                    handle->markAsHavingCrossOriginRedirect();

                ResourceRequest redirectedRequest = request.resourceRequest();
                redirectedRequest.setURL(WTF::move(redirectedURL));
                // Recompute the Cookie header after approval for the redirected
                // URL instead of forwarding cookies from the previous request.
                redirectedRequest.removeHTTPHeaderField(HTTPHeaderName::Cookie);
                // Apply HTTP redirect semantics (301/302/303 may turn POST into
                // GET and drop the upload body; 307/308 must preserve it). Without
                // this, form submissions and login flows can be replayed incorrectly
                // when a server redirects the request.
                redirectedRequest.redirectAsGETIfNeeded(request.resourceRequest(), response);

                Ref<WKCResourceHandleCurlClient> protectedThis { *this };
                handle->client()->willSendRequestAsync(handle.get(), WTF::move(redirectedRequest), WTF::move(response),
                    [protectedThis = WTF::move(protectedThis)](ResourceRequest&& approvedRequest) mutable {
                        if (approvedRequest.isNull()) {
                            if (protectedThis->m_handle)
                                protectedThis->m_handle->cancel();
                            return;
                        }
                        protectedThis->restartWithRequest(WTF::move(approvedRequest));
                    });
                return;
            }
        }

        Ref<CurlRequest> protectedRequest { request };
        handle->didReceiveResponse(WTF::move(response), [protectedRequest = WTF::move(protectedRequest)]() mutable {
            protectedRequest->completeDidReceiveResponse();
        });
    }

    void curlDidReceiveData(CurlRequest&, Ref<SharedBuffer>&& buffer) final
    {
        if (!m_handle || !m_handle->client())
            return;

        auto length = std::min<size_t>(buffer->size(), INT_MAX);
        m_handle->client()->didReceiveData(m_handle.get(), buffer.get(), static_cast<int>(length));
    }

    void curlDidComplete(CurlRequest&, NetworkLoadMetrics&& metrics) final
    {
        RefPtr<ResourceHandle> handle = m_handle;
        if (handle && handle->client())
            handle->client()->didFinishLoading(handle.get(), metrics);
        removeActiveTransfer(handle.get());
    }

    void curlDidFailWithError(CurlRequest&, ResourceError&& error, CertificateInfo&&) final
    {
        RefPtr<ResourceHandle> handle = m_handle;
        if (handle && handle->client())
            handle->client()->didFail(handle.get(), error);
        removeActiveTransfer(handle.get());
    }

    void restartWithRequest(ResourceRequest&& request)
    {
        RefPtr<ResourceHandle> handle = m_handle;
        if (!handle || request.isNull() || !handle->client()) {
            if (handle)
                handle->cancel();
            return;
        }

        auto iterator = activeTransfers().find(handle.get());
        if (iterator == activeTransfers().end())
            return;

        // Cancel the old transfer only after WebCore has approved the redirect.
        iterator->second.request->cancel();
        applyCookiesToRequest(handle->context(), request);
        auto nextRequest = CurlRequest::create(request, *this, CurlRequest::CaptureNetworkLoadMetrics::Extended);
        if (sameOrigin(m_originalURL, request.url()))
            nextRequest->setUserPass(m_user, m_password);

        iterator->second.request = WTF::move(nextRequest);
        iterator->second.request->resume();
    }

private:
    static constexpr unsigned maximumRedirects = 20;

    RefPtr<ResourceHandle> m_handle;
    URL m_originalURL;
    String m_user;
    String m_password;
};

struct SynchronousLoadContext {
    Vector<uint8_t>* data { nullptr };
    std::vector<std::string> headerLines;
};

size_t synchronousWriteCallback(char* contents, size_t size, size_t count, void* userData)
{
    auto bytes = size * count;
    auto& context = *static_cast<SynchronousLoadContext*>(userData);
    context.data->append(std::span<const uint8_t> { reinterpret_cast<const uint8_t*>(contents), bytes });
    return bytes;
}

size_t synchronousHeaderCallback(char* contents, size_t size, size_t count, void* userData)
{
    auto bytes = size * count;
    auto& context = *static_cast<SynchronousLoadContext*>(userData);
    context.headerLines.emplace_back(contents, bytes);
    return bytes;
}

String trimHeaderValue(const std::string& value)
{
    size_t start = 0;
    while (start < value.size() && (value[start] == ' ' || value[start] == '\t' || value[start] == '\r' || value[start] == '\n'))
        ++start;

    size_t end = value.size();
    while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r' || value[end - 1] == '\n'))
        --end;

    return String::fromUTF8(std::span<const char> { value.data() + start, end - start });
}

void applySynchronousHeaders(const std::vector<std::string>& headers, ResourceResponse& response)
{
    for (const auto& line : headers) {
        // Ignore status lines and blank separators. libcurl may emit a separate
        // block for a proxy handshake or interim response; the final status is
        // queried from the easy handle after the transfer finishes.
        if (line.rfind("HTTP/", 0) == 0)
            continue;

        auto colon = line.find(':');
        if (colon == std::string::npos)
            continue;

        auto name = trimHeaderValue(line.substr(0, colon));
        auto value = trimHeaderValue(line.substr(colon + 1));
        if (!name.isEmpty())
            response.addHTTPHeaderField(name, value);
    }
}

} // anonymous namespace

ResourceHandleInternal::~ResourceHandleInternal() = default;

ResourceHandle::~ResourceHandle()
{
    cancel();
}

bool ResourceHandle::start()
{
    if (!client() || !firstRequest().url().isValid())
        return false;

    auto& transfers = activeTransfers();
    if (transfers.find(this) != transfers.end())
        return true;

    auto curlClient = adoptRef(*new WKCResourceHandleCurlClient(*this, d->m_user, d->m_password));
    ResourceRequest initialRequest = firstRequest();
    applyCookiesToRequest(context(), initialRequest);
    auto request = CurlRequest::create(initialRequest, curlClient.get(), CurlRequest::CaptureNetworkLoadMetrics::Extended);
    if (!d->m_user.isEmpty() || !d->m_password.isEmpty())
        request->setUserPass(d->m_user, d->m_password);

    auto result = transfers.emplace(this, ActiveResourceTransfer(WTF::move(request), WTF::move(curlClient)));
    result.first->second.request->resume();
    return true;
}

void ResourceHandle::cancel()
{
    auto& transfers = activeTransfers();
    auto iterator = transfers.find(this);
    if (iterator == transfers.end())
        return;

    iterator->second.request->cancel();
    transfers.erase(iterator);
}

void ResourceHandle::platformSetDefersLoading(bool)
{
    // CurlRequest currently exposes start/cancel and response-resume, but not
    // a persistent pause/resume API. This hook is intentionally harmless until
    // WKC gains a loader-level scheduling policy.
}

void ResourceHandle::platformLoadResourceSynchronously(NetworkingContext*, const ResourceRequest& request, StoredCredentialsPolicy, SecurityOrigin*, ResourceError& error, ResourceResponse& response, Vector<uint8_t>& data)
{
    data.clear();
    response = ResourceResponse();

    // Synchronous XHR and form-backed requests can carry a request body too.
    // Support in-memory body elements here. File/blob-backed elements still need
    // an asynchronous upload stream, so fail explicitly rather than silently
    // flattening them into an incomplete request.
    Vector<uint8_t> requestBodyBytes;
    auto requestBody = request.httpBody();
    if (requestBody) {
        for (const auto& element : requestBody->elements()) {
            if (!std::get_if<Vector<uint8_t>>(&element.data)) {
                error = ResourceError(CURLE_READ_ERROR, request.url(), ResourceError::Type::General);
                return;
            }
        }
        requestBodyBytes = requestBody->flatten();
    }

    CurlHandle curl;
    curl.setURL(request.url(), CurlHandle::LocalhostAlias::Disable);
    curl.setTimeout(CurlContext::singleton().defaultTimeoutInterval());
    curl.appendRequestHeaders(request.httpHeaderFields());

    auto method = request.httpMethod();
    if (method.isEmpty() || method == "GET"_s)
        curl.enableHttpGetRequest();
    else if (method == "HEAD"_s)
        curl.enableHttpHeadRequest();
    else
        curl.setHttpCustomRequest(method);

    if (requestBody) {
        const char* bodyData = requestBodyBytes.isEmpty()
            ? ""
            : reinterpret_cast<const char*>(requestBodyBytes.span().data());
        auto optionResult = curl_easy_setopt(curl.handle(), CURLOPT_POSTFIELDS, bodyData);
        if (optionResult == CURLE_OK)
            optionResult = curl_easy_setopt(curl.handle(), CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(requestBodyBytes.size()));
        if (optionResult != CURLE_OK) {
            data.clear();
            error = ResourceError(static_cast<int>(optionResult), request.url(), ResourceError::Type::General);
            return;
        }
    }

    SynchronousLoadContext context { &data, { } };
    curl.setWriteCallbackFunction(synchronousWriteCallback, &context);
    curl.setHeaderCallbackFunction(synchronousHeaderCallback, &context);

    auto result = curl.perform();
    response.setURL(URL { request.url() });
    response.setSource(ResourceResponse::Source::Network);
    applySynchronousHeaders(context.headerLines, response);

    long statusCode = 0;
    curl_easy_getinfo(curl.handle(), CURLINFO_RESPONSE_CODE, &statusCode);
    response.setHTTPStatusCode(static_cast<int>(statusCode));
    response.setExpectedContentLength(static_cast<long long>(data.size()));

    auto contentType = response.httpHeaderField(HTTPHeaderName::ContentType);
    response.setMimeType(extractMIMETypeFromMediaType(contentType).convertToASCIILowercase());
    response.setTextEncodingName(extractCharsetFromMediaType(contentType).toString());

    if (result == CURLE_OK)
        error = ResourceError();
    else
        error = ResourceError(static_cast<int>(result), request.url(), result == CURLE_OPERATION_TIMEDOUT ? ResourceError::Type::Timeout : ResourceError::Type::General);
}

bool ResourceHandle::shouldUseCredentialStorage()
{
    return (!client() || client()->shouldUseCredentialStorage(this)) && firstRequest().url().protocolIsInHTTPFamily();
}

void ResourceHandle::didReceiveAuthenticationChallenge(const AuthenticationChallenge& challenge)
{
    if (client())
        client()->didReceiveAuthenticationChallenge(this, challenge);
}

void ResourceHandle::receivedCredential(const AuthenticationChallenge&, const Credential&)
{
}

void ResourceHandle::receivedRequestToContinueWithoutCredential(const AuthenticationChallenge&)
{
    clearAuthentication();
}

void ResourceHandle::receivedCancellation(const AuthenticationChallenge& challenge)
{
    if (client())
        client()->receivedCancellation(this, challenge);
}

void ResourceHandle::receivedRequestToPerformDefaultHandling(const AuthenticationChallenge&)
{
}

void ResourceHandle::receivedChallengeRejection(const AuthenticationChallenge&)
{
    clearAuthentication();
}

} // namespace WebCore

#endif // USE(CURL)
