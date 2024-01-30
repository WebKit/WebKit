/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace WTR {

struct TestCommand;

struct TestFeatures {
    std::unordered_map<std::string, bool> boolWebPreferenceFeatures;
    std::unordered_map<std::string, double> doubleWebPreferenceFeatures;
    std::unordered_map<std::string, uint32_t> uint32WebPreferenceFeatures;
    std::unordered_map<std::string, std::string> stringWebPreferenceFeatures;

    std::unordered_map<std::string, bool> boolTestRunnerFeatures;
    std::unordered_map<std::string, double> doubleTestRunnerFeatures;
    std::unordered_map<std::string, uint16_t> uint16TestRunnerFeatures;
    std::unordered_map<std::string, std::string> stringTestRunnerFeatures;
    std::unordered_map<std::string, std::vector<std::string>> stringVectorTestRunnerFeatures;
};

bool operator==(const TestFeatures&, const TestFeatures&);

void merge(TestFeatures& base, TestFeatures additional);

TestFeatures hardcodedFeaturesBasedOnPathForTest(const TestCommand&);

enum class TestHeaderKeyType : uint8_t {
    BoolWebPreference,
    DoubleWebPreference,
    UInt32WebPreference,
    StringWebPreference,

    BoolTestRunner,
    DoubleTestRunner,
    UInt16TestRunner,
    StringTestRunner,
    StringRelativePathTestRunner,
    StringURLTestRunner,
    StringVectorTestRunner,

    Unknown
};
TestFeatures featureDefaultsFromTestHeaderForTest(const TestCommand&, const std::unordered_map<std::string, TestHeaderKeyType>&);
TestFeatures featureDefaultsFromSelfComparisonHeader(const TestCommand&, const std::unordered_map<std::string, TestHeaderKeyType>&);
TestFeatures featureFromAdditionalHeaderOption(const TestCommand&, const std::unordered_map<std::string, TestHeaderKeyType>&);

}
