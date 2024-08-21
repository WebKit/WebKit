#include "OpenID4VPRequest.h"
#include "Document.h"
#include "SecurityOrigin.h"
#include <wtf/URL.h>

namespace WebCore {

String OpenID4VPRequest::validate(Document& document)
{
    if (expected_origins.isEmpty() && !isSignedRequest())
        return "At least one expected origin must be specified."_s;

    return {};
}

bool OpenID4VPRequest::isSignedRequest()
{
    return true;
}

String OpenID4VPRequest::processExpectedOrigins(Document& document)
{
    Vector<String> normalizedOrigins;

    for (auto& origin : expected_origins) {
        if (origin.isEmpty())
            return "Expected origin must not be empty."_s;

        auto url = SecurityOrigin::create(document.completeURL(origin));

        if (!url.isValid())
            return "Expected origin is not a valid URL."_s;

        if (!url.protocolIs("https"))
            return "Expected origin must be an HTTPS URL."_s;

        normalizedOrigins.append(url->toString());
    }
    return {};
} // namespace WebCore
