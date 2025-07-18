/*
 * Copyright (C) 2025 Shopify Inc. All rights reserved.
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
#include "IntegrityPolicy.h"

#include "Document.h"
#include "IntegrityPolicyViolationReportBody.h"
#include "RFC8941.h"
#include "Report.h"
#include "ResourceResponse.h"
#include "SecurityContext.h"
#include "SubresourceIntegrity.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {

// https://w3c.github.io/webappsec-subresource-integrity/#report-violation
static void reportViolation(const URL& requestURL, Document& document, bool block, bool reportBlock, const IntegrityPolicy* integrityPolicy, const IntegrityPolicy* integrityPolicyReportOnly)
{
    auto sendReport = [&document, &requestURL](bool reportOnly, const Vector<String>& endpoints) {
        for (const auto& endpoint : endpoints) {
            const URL& documentURL = document.url();

            auto reportBody = IntegrityPolicyViolationReportBody::create(documentURL.string(), requestURL.string(), "script"_s, reportOnly);
            Ref reportFormData = Report::createReportFormDataForViolation("integrity-violation"_s, documentURL, document.httpUserAgent(), endpoint, [&](auto& body) {
                body.setString("documentURL"_s, documentURL.string());
                body.setString("blockedURL"_s, requestURL.string());
                body.setString("destination"_s, "script"_s);
                body.setBoolean("reportOnly"_s, reportOnly);
            });
            document.sendReportToEndpoints(documentURL, { }, singleElementSpan(endpoint), WTFMove(reportFormData), ViolationReportType::IntegrityPolicy);
            document.notifyReportObservers(Report::create(reportBody->type(), reportBody->documentURL(), reportBody.copyRef()));
        }
    };
    if (block)
        sendReport(false, integrityPolicy->endpoints);
    if (reportBlock)
        sendReport(true, integrityPolicyReportOnly->endpoints);
}

// https://w3c.github.io/webappsec-subresource-integrity/#processing-an-integrity-policy
std::unique_ptr<IntegrityPolicy> processIntegrityPolicy(const ResourceResponse& response, HTTPHeaderName headerName)
{
    auto parseInnerList = [](auto& innerList, auto& vector, std::optional<String> match) {
        for (auto& itemAndParameters : innerList) {
            auto& item = itemAndParameters.first;
            auto* token = std::get_if<RFC8941::Token>(&item);
            if (!token)
                continue;
            auto value = token->string();

            if ((!match || value == *match))
                vector.append(value);
        }
    };
    // 2. Let dictionary be the result of getting a structured field value from headers given headerName and "dictionary".
    auto parsedHeader = RFC8941::parseDictionaryStructuredFieldValue(response.httpHeaderField(headerName));
    if (!parsedHeader)
        return nullptr;
    // 1. Let integrityPolicy be a new integrity policy struct.
    std::unique_ptr<IntegrityPolicy> policy = makeUnique<IntegrityPolicy>();
    bool sourcesExists = false;
    for (auto& [name, valueAndParameters] : *parsedHeader) {
        RFC8941::InnerList* innerList = std::get_if<RFC8941::InnerList>(&valueAndParameters.first);
        if (!innerList)
            continue;
        // 4. If dictionary["blocked-destinations"] exists:
        // 4.1. If its value contains "script", append "script" to integrityPolicy’s blocked destinations.
        if (name == "blocked-destinations"_s)
            parseInnerList(*innerList, policy->blockedDestinations, "script"_s);
        // 3. If dictionary["sources"] ... value contains "inline", append "inline" to integrityPolicy’s sources.
        else if (name == "sources"_s) {
            sourcesExists = true;
            parseInnerList(*innerList, policy->sources, "inline"_s);
        } else if (name == "endpoints"_s)
            parseInnerList(*innerList, policy->endpoints, std::nullopt);
    }
    // 3. If dictionary["sources"] does not exist, append "inline" to integrityPolicy’s sources.
    if (!sourcesExists)
        policy->sources.append("inline"_s);
    return policy;
}

// https://w3c.github.io/webappsec-subresource-integrity/#should-request-be-blocked-by-integrity-policy-section
bool shouldRequestBeBlockedByIntegrityPolicy(ScriptExecutionContext& context, const ResourceLoaderOptions& options, const URL& url)
{
    auto integrityPolicy = context.integrityPolicy();
    auto integrityPolicyReportOnly = context.integrityPolicyReportOnly();

    // 2. Let parsedMetadata be the result of calling parse metadata with request’s integrity metadata.
    auto parsedMetadata = parseIntegrityMetadata(options.integrity);
    // 3. If parsedMetadata is not the empty set and request’s mode is either "cors" or "same-origin", return "Allowed".
    // 4. If request’s url is local, return "Allowed".
    if ((parsedMetadata && parsedMetadata->size() && options.mode != FetchOptions::Mode::NoCors) || url.hasLocalScheme())
        return false;
    // 7. If both policy and reportPolicy are empty integrity policy structs, return "Allowed".
    if (!integrityPolicy && !integrityPolicyReportOnly)
        return false;
    // We don't currently support anything but script
    if (options.destination != FetchOptionsDestination::Script)
        return false;

    // 10. Let block be a boolean, initially false.
    bool block = false;
    // 11. Let reportBlock be a boolean, initially false.
    bool reportBlock = false;
    // 12. If policy’s sources contains "inline" and policy’s blocked destinations contains request’s destination, set block to true.
    if (integrityPolicy && integrityPolicy->sources.contains("inline"_s) && integrityPolicy->blockedDestinations.contains("script"_s))
        block = true;
    // 13. If reportPolicy’s sources contains "inline" and reportPolicy’s blocked destinations contains request’s destination, set reportBlock to true.
    if (integrityPolicyReportOnly && integrityPolicyReportOnly->sources.contains("inline"_s) && integrityPolicyReportOnly->blockedDestinations.contains("script"_s))
        reportBlock = true;

    if (block || reportBlock) {
        Ref document = downcast<Document>(context);
        reportViolation(url, document, block, reportBlock, integrityPolicy, integrityPolicyReportOnly);
    }

    return block;
}
} // namespace WebCore
