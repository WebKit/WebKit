// Copyright © 2025  All rights reserved.

#pragma once

#include <optional>

namespace WebCore {

struct ContentRuleListMatchedRule {
    struct MatchedRule {
        double ruleId;
        String rulesetId;
        std::optional<String> extensionId;
    };

    struct Request {
        double frameId;
        double parentFrameId;
        String method;
        String requestId;
        double tabId;
        String type;
        String url;
        std::optional<String> initiator;
        std::optional<String> documentId;
        std::optional<String> documentLifecycle;
        std::optional<String> frameType;
        std::optional<String> parentDocumentId;
    };

    ContentRuleListMatchedRule(MatchedRule rule, Request request)
        : rule(rule)
        , request(request)
    {
    }

    MatchedRule rule;
    Request request;
};

} // namespace WebCore
