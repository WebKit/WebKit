#pragma once

#include <wtf/Forward.h>
#include <wtf/Vector.h>

namespace WebCore {

class URL;
class ResourceRequest;

struct CompressionDictionary {
    String match;
    Vector<String> matchDest;
    URL dictionaryURL;
    double fetchTime; // UNIX timestamp or monotonic time
};

bool isValidDictionaryPattern(const String& match, const URL& dictionaryURL);
bool doesRequestMatchDictionary(const ResourceRequest&, const CompressionDictionary&);
std::optional<CompressionDictionary> selectBestMatchingDictionary(const ResourceRequest&, const Vector<CompressionDictionary>&);

}
// Copyright © 2025  All rights reserved.

