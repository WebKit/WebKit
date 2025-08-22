// Copyright © 2025  All rights reserved.

#pragma once

#include <optional>

namespace WebCore {

struct ContentRuleListMatchedRule {
    struct Request {
        std::optional<String> documentId;
        std::optional<String> documentLifecycle;
        std::optional<double> frameId;
        std::optional<String> frameType;
        std::optional<String> initiator;
        std::optional<String> method;
        std::optional<String> parentDocumentId;
        std::optional<double> parentFrameId;
        std::optional<String> requestId;
        std::optional<String> type;
        std::optional<String> url;
    };

    struct MatchedRule {
        std::optional<String> extensionId;
        std::optional<double> ruleId;
        std::optional<String> rulesetId;
    };

    Request request;
    MatchedRule rule;
};

} // namespace WebCore
