// Copyright © 2025  All rights reserved.

#pragma once

#include <optional>

namespace WebCore {

struct ContentRuleListMatchedRule {
    struct MatchedRule {
        double ruleId;
        RefPtr<StringImpl> rulesetId;
        std::optional<RefPtr<StringImpl>> extensionId;
    };

    struct Request {
        double frameId;
        double parentFrameId;
        RefPtr<StringImpl> method;
        RefPtr<StringImpl> requestId;
        double tabId;
        RefPtr<StringImpl> type;
        RefPtr<StringImpl> url;
        std::optional<RefPtr<StringImpl>> initiator;
        std::optional<RefPtr<StringImpl>> documentId;
        std::optional<RefPtr<StringImpl>> documentLifecycle;
        std::optional<RefPtr<StringImpl>> frameType;
        std::optional<RefPtr<StringImpl>> parentDocumentId;
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
