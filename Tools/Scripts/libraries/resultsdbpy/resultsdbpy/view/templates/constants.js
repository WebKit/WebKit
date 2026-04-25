// Copyright (C) 2023 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

const XCODE_CLOUD_SUITES = [{% for suite in XcodeCloud %}
    '{{ suite }}',
{% endfor %}];
const DEFAULT_ARCHITECTURE = {{ default_architecture }};
const TESTS_LIMITS = JSON.parse('{{ tests_limits|safe }}');
const SUITES_LIMITS = JSON.parse('{{ suites_limits|safe }}');
const COMMITS_LIMITS = JSON.parse('{{ commits_limits|safe }}');
const DASHBOARD_QUERY = JSON.parse('{{ dashboard_queries|safe }}');

export {XCODE_CLOUD_SUITES, DEFAULT_ARCHITECTURE, TESTS_LIMITS, SUITES_LIMITS, COMMITS_LIMITS, DASHBOARD_QUERY}
