/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "NetworkCaptureManager.h"

#if ENABLE(NETWORK_CAPTURE)

#include "NetworkCaptureLogging.h"
#include "NetworkCaptureResource.h"
#include <WebCore/ResourceRequest.h>
#include <algorithm>
#include <iterator>
#include <limits>
#include <wtf/MD5.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/URL.h>
#include <wtf/text/Base64.h>
#include <wtf/text/StringBuilder.h>

#define DEBUG_CLASS Manager

namespace WebKit {
namespace NetworkCapture {

using namespace WebCore::FileSystem;

static const char* kDirNameRecordReplay = "WebKitPerf/record_replay";
static const char* kDirNameResources = "resources";
static const char* kFileNameReportLoad = "report_load.txt";
static const char* kFileNameReportRecord = "report_record.txt";
static const char* kFileNameReportReplay = "report_replay.txt";

static int kMaxMatch = std::numeric_limits<int>::max();
static int kMinMatch = std::numeric_limits<int>::min();

Manager& Manager::singleton()
{
    static NeverDestroyed<Manager> instance;
    return instance;
}

void Manager::initialize(const String& recordReplayMode, const String& recordReplayCacheLocation)
{
    if (equalIgnoringASCIICase(recordReplayMode, "record")) {
        DEBUG_LOG("Initializing: recording mode");
        m_recordReplayMode = Record;
    } else if (equalIgnoringASCIICase(recordReplayMode, "replay")) {
        DEBUG_LOG("Initializing: replay mode");
        m_recordReplayMode = Replay;
    } else {
        DEBUG_LOG("Initializing: disabled");
        m_recordReplayMode = Disabled;
    }

    m_recordReplayCacheLocation = pathByAppendingComponent(recordReplayCacheLocation, kDirNameRecordReplay);
    DEBUG_LOG("Cache location = " STRING_SPECIFIER, DEBUG_STR(m_recordReplayCacheLocation));

    if (isRecording()) {
        m_recordFileHandle = WebCore::FileHandle(reportRecordPath(), FileOpenMode::Write);
    } else if (isReplaying()) {
        m_recordFileHandle = WebCore::FileHandle(reportRecordPath(), FileOpenMode::Read);
        m_loadFileHandle = WebCore::FileHandle(reportLoadPath(), FileOpenMode::Write);
        m_replayFileHandle = WebCore::FileHandle(reportReplayPath(), FileOpenMode::Write);
        loadResources();
    }
}

void Manager::terminate()
{
    m_loadFileHandle.close();
    m_recordFileHandle.close();
    m_replayFileHandle.close();
}

Resource* Manager::findMatch(const WebCore::ResourceRequest& request)
{
    DEBUG_LOG_VERBOSE("URL = " STRING_SPECIFIER, DEBUG_STR(request.url().string()));

    auto bestMatch = findExactMatch(request);
    if (!bestMatch)
        bestMatch = findBestFuzzyMatch(request);

#if CAPTURE_INTERNAL_DEBUGGING
    if (!bestMatch)
        DEBUG_LOG("Could not find match for: " STRING_SPECIFIER, DEBUG_STR(request.url().string()));
    else if (request.url() == bestMatch->url())
        DEBUG_LOG("Found exact match for: " STRING_SPECIFIER, DEBUG_STR(request.url().string()));
    else {
        DEBUG_LOG("Found fuzzy match for: " STRING_SPECIFIER, DEBUG_STR(request.url().string()));
        DEBUG_LOG("       replaced with : " STRING_SPECIFIER, DEBUG_STR(bestMatch->url().string()));
    }
#endif

    return bestMatch;
}

Resource* Manager::findExactMatch(const WebCore::ResourceRequest& request)
{
    const auto& url = request.url();
    auto lower = std::lower_bound(std::begin(m_cachedResources), std::end(m_cachedResources), url, [](auto& resource, const auto& url) {
        return WTF::codePointCompareLessThan(resource.url().string(), url.string());
    });

    if (lower != std::end(m_cachedResources) && lower->url() == url) {
        DEBUG_LOG_VERBOSE("Found exact match: " STRING_SPECIFIER, DEBUG_STR(lower->url().string()));
        return &*lower;
    }

    return nullptr;
}

Resource* Manager::findBestFuzzyMatch(const WebCore::ResourceRequest& request)
{
    const auto& url = request.url();
    const auto& urlIdentifyingCommonDomain = Manager::urlIdentifyingCommonDomain(url);

    const auto& lower = std::lower_bound(std::begin(m_cachedResources), std::end(m_cachedResources), urlIdentifyingCommonDomain, [](auto& resource, const auto& urlIdentifyingCommonDomain) {
        return WTF::codePointCompareLessThan(resource.urlIdentifyingCommonDomain(), urlIdentifyingCommonDomain);
    });
    const auto& upper = std::upper_bound(lower, std::end(m_cachedResources), urlIdentifyingCommonDomain, [](const auto& urlIdentifyingCommonDomain, auto& resource) {
        return WTF::codePointCompareLessThan(urlIdentifyingCommonDomain, resource.urlIdentifyingCommonDomain());
    });

    Resource* bestMatch = nullptr;
    int bestScore = kMinMatch;
    const auto& requestParameters = WTF::URLParser::parseURLEncodedForm(url.query());
    for (auto iResource = lower; iResource != upper; ++iResource) {
        int thisScore = fuzzyMatchURLs(url, requestParameters, iResource->url(), iResource->queryParameters());
        // TODO: Consider ignoring any matches < 0 as being too different.
        if (bestScore < thisScore) {
            DEBUG_LOG("New best match (%d): " STRING_SPECIFIER, thisScore, DEBUG_STR(iResource->url().string()));
            bestScore = thisScore;
            bestMatch = &*iResource;
            if (bestScore == kMaxMatch)
                break;
        }
    }

    return bestMatch;
}

// TODO: Convert to an interface based on ResourceRequest so that we can do
// deeper matching.

int Manager::fuzzyMatchURLs(const URL& requestURL, const WTF::URLParser::URLEncodedForm& requestParameters, const URL& resourceURL, const WTF::URLParser::URLEncodedForm& resourceParameters)
{
    // TODO: consider requiring that any trailing suffixes (e.g., ".js",
    // ".png", ".css", ".html", etc.) should be an exact match.

    // We do fuzzy matching on the path and query parameters. So let's first
    // make sure that all the other parts are equal.

    // If scheme, host, and port don't all match, return this as the "worst"
    // match.

    if (!protocolHostAndPortAreEqual(requestURL, resourceURL)) {
        DEBUG_LOG("Scheme/host/port mismatch: " STRING_SPECIFIER " != " STRING_SPECIFIER, DEBUG_STR(requestURL.string()), DEBUG_STR(resourceURL.string()));
        return kMinMatch;
    }

    // If fragments don't match, return this as the "worst" match.

    if (requestURL.fragmentIdentifier() != resourceURL.fragmentIdentifier()) {
        DEBUG_LOG("Fragments mismatch: " STRING_SPECIFIER " != " STRING_SPECIFIER, DEBUG_STR(requestURL.string()), DEBUG_STR(resourceURL.string()));
        return kMinMatch;
    }

    DEBUG_LOG("Fuzzy matching:");
    DEBUG_LOG("   : " STRING_SPECIFIER, DEBUG_STR(requestURL.string()));
    DEBUG_LOG("   : " STRING_SPECIFIER, DEBUG_STR(resourceURL.string()));

    // Compare the path components and the query parameters. Score each partial
    // match as +4, each mismatch as -1, and each missing component as -1.
    //
    // Note that at the current time these values are rather arbitrary and
    // could fine-tuned.

    const int kPathMatchScore = 4;
    const int kPathMismatchScore = -1;
    const int kPathMissingScore = -1;
    const int kParameterMatchScore = 4;
    const int kParameterMismatchScore = -1;
    const int kParameterMissingScore = -1;

    int score = 0;

    // Quantize the differences in URL paths.
    //
    // The approach here is to increase our score for each matching path
    // component, and to subtract for each differing component as well as for
    // components that exist in one path but not the other.

    const auto& requestPath = requestURL.path();
    const auto& resourcePath = resourceURL.path();

    Vector<String> requestPathComponents = requestPath.split('/');
    Vector<String> resourcePathComponents = resourcePath.split('/');

    auto updatedIterators = std::mismatch(
        std::begin(requestPathComponents), std::end(requestPathComponents),
        std::begin(resourcePathComponents), std::end(resourcePathComponents));

    auto matchingDistance = std::distance(std::begin(requestPathComponents), updatedIterators.first);
    auto requestPathMismatchDistance = std::distance(updatedIterators.first, std::end(requestPathComponents));
    auto resourcePathMismatchDistance = std::distance(updatedIterators.second, std::end(resourcePathComponents));
    decltype(matchingDistance) mismatchingDistance;
    decltype(matchingDistance) missingDistance;
    if (requestPathMismatchDistance < resourcePathMismatchDistance) {
        mismatchingDistance = requestPathMismatchDistance;
        missingDistance = resourcePathMismatchDistance - requestPathMismatchDistance;
    } else {
        mismatchingDistance = resourcePathMismatchDistance;
        missingDistance = requestPathMismatchDistance - resourcePathMismatchDistance;
    }

    DEBUG_LOG("Path matching results: matching = %d, mismatching = %d, missing = %d",
        static_cast<int>(matchingDistance),
        static_cast<int>(mismatchingDistance),
        static_cast<int>(missingDistance));

    score += matchingDistance * kPathMatchScore
        + mismatchingDistance * kPathMismatchScore
        + missingDistance * kPathMissingScore;
    DEBUG_LOG("Score = %d", score);

    // Quantize the differences in query parameters.
    //
    // The approach here is to walk lock-step over the two sets of query
    // parameters. For each pair of parameters for each URL, we compare their
    // names and values. If the names and values match, we add a high score. If
    // just the names match, we add a lower score.
    //
    // If the names don't match, we then assume that some intervening query
    // parameters have been added to one or the other URL. We therefore try to
    // sync up the iterators used to traverse the query parameter collections
    // so that they're again pointing to parameters with the same names. We
    // first start scanning forward down the query parameters for one URL,
    // looking for one with the same name as the one we're on in the other URL.
    // If that doesn't turn up a match, we reverse the roles of the query
    // parameters perform the same process of scanning forward. If neither of
    // these scans produces a match, we figure that each query parameter we're
    // looking at from each of the query parameter collections is unique. We
    // deduct points from the overall score and move on to the next query
    // parameters in each set.
    //
    // If, on the other hand, the forward-scanning does turn up a match, we
    // adjust out iterators so that they're now again pointing to query
    // parameters with the same name. This synchronization involves skipping
    // over any intervening query parameters in one collection or the other.
    // The assumption here is that these intervening query parameters are
    // insertions that exist in one URL but not the other. We treat them as
    // such, subtracting from the overall score for each one. However, this
    // assumption might easily be incorrect. It might be that the query
    // parameters that we're skipping over in one URL might exist in the other
    // URL. If so, then we are foregoing the possibility of using those matches
    // to increase the overall match score between the two URLs.
    //
    // To address this problem, we might want to consider sorting the query
    // parameters by their names. However, doing this may cause problems if the
    // order of the parameters is significant. So if we decide to take the
    // approach of sorting the parameters, keep in mind this possible drawback.

    auto requestParameter = std::begin(requestParameters);
    auto resourceParameter = std::begin(resourceParameters);

    for (; requestParameter != std::end(requestParameters) && resourceParameter != std::end(resourceParameters); ++requestParameter, ++resourceParameter) {
        if (requestParameter->key == resourceParameter->key) {
#if CAPTURE_INTERNAL_DEBUGGING
            if (requestParameter->value == resourceParameter->value)
                DEBUG_LOG("Matching parameter names and values: \"" STRING_SPECIFIER "\" = \"" STRING_SPECIFIER "\"", DEBUG_STR(requestParameter->first), DEBUG_STR(requestParameter->second));
            else
                DEBUG_LOG("Mismatching parameter values: \"" STRING_SPECIFIER "\" = \"" STRING_SPECIFIER "\" vs. \"" STRING_SPECIFIER "\"", DEBUG_STR(requestParameter->first), DEBUG_STR(requestParameter->second), DEBUG_STR(resourceParameter->second));
#endif
            score += (requestParameter->value == resourceParameter->value) ? kParameterMatchScore : kParameterMismatchScore;
            DEBUG_LOG("Score = %d", score);
        } else {
            DEBUG_LOG("Mismatching parameter names: " STRING_SPECIFIER ", " STRING_SPECIFIER, DEBUG_STR(requestParameter->first), DEBUG_STR(resourceParameter->first));

            const auto scanForwardForMatch = [&score, kParameterMatchScore, kParameterMismatchScore, kParameterMissingScore](const auto& fixedIter, auto& scanningIter, const auto& scannerEnd) {
                auto scanner = scanningIter;
                while (scanner != scannerEnd && scanner->key != fixedIter->key)
                    ++scanner;
                if (scanner == scannerEnd)
                    return false;
                DEBUG_LOG("Skipping past %d non-matching parameter names", static_cast<int>(std::distance(scanningIter, scanner)));
                score += kParameterMissingScore * std::distance(scanningIter, scanner);
                DEBUG_LOG("Score = %d", score);
#if CAPTURE_INTERNAL_DEBUGGING
                if (fixedIter->second == scanner->second)
                    DEBUG_LOG("Matching parameter names and values: \"" STRING_SPECIFIER "\" = \"" STRING_SPECIFIER "\"", DEBUG_STR(fixedIter->first), DEBUG_STR(fixedIter->second));
                else
                    DEBUG_LOG("Mismatching parameter values: \"" STRING_SPECIFIER "\" = \"" STRING_SPECIFIER "\" vs. \"" STRING_SPECIFIER "\"", DEBUG_STR(fixedIter->first), DEBUG_STR(fixedIter->second), DEBUG_STR(scanner->second));
#endif
                score += (fixedIter->value == scanner->value) ? kParameterMatchScore : kParameterMismatchScore;
                DEBUG_LOG("Score = %d", score);
                scanningIter = scanner;
                return true;
            };

            if (!scanForwardForMatch(requestParameter, resourceParameter, std::end(resourceParameters))) {
                if (!scanForwardForMatch(resourceParameter, requestParameter, std::end(requestParameters))) {
                    DEBUG_LOG("Unmatched parameter: " STRING_SPECIFIER "=" STRING_SPECIFIER, DEBUG_STR(requestParameter->first), DEBUG_STR(requestParameter->second));
                    DEBUG_LOG("Unmatched parameter: " STRING_SPECIFIER "=" STRING_SPECIFIER, DEBUG_STR(resourceParameter->first), DEBUG_STR(resourceParameter->second));
                    score += kParameterMissingScore + kParameterMissingScore;
                    DEBUG_LOG("Score = %d", score);
                }
            }
        }
    }

    DEBUG_LOG("Adjusting for trailing parameters");
    score += kParameterMissingScore
        * (std::distance(requestParameter, std::end(requestParameters))
            + std::distance(resourceParameter, std::end(resourceParameters)));
    DEBUG_LOG("Score = %d", score);

    return score;
}

void Manager::loadResources()
{
    auto lines = readFile(reportRecordPath());
    if (!lines)
        return;

    for (const auto& line : *lines) {
        if (line.size() != 2) {
            DEBUG_LOG_ERROR("line.size == %d", (int) line.size());
            continue;
        }

        Resource newResource(hashToPath(line[0]));
        m_cachedResources.append(WTFMove(newResource));
    }

    std::sort(std::begin(m_cachedResources), std::end(m_cachedResources), [](auto& left, auto& right) {
        return WTF::codePointCompareLessThan(left.url().string(), right.url().string());
    });

    for (auto& resource : m_cachedResources)
        logLoadedResource(resource);
}

String Manager::reportLoadPath()
{
    return pathByAppendingComponent(m_recordReplayCacheLocation, kFileNameReportLoad);
}

String Manager::reportRecordPath()
{
    return pathByAppendingComponent(m_recordReplayCacheLocation, kFileNameReportRecord);
}

String Manager::reportReplayPath()
{
    return pathByAppendingComponent(m_recordReplayCacheLocation, kFileNameReportReplay);
}

String Manager::requestToPath(const WebCore::ResourceRequest& request)
{
    // TODO: come up with a more comprehensive hash that includes HTTP method
    // and possibly other values (such as headers).

    const auto& hash = stringToHash(request.url().string());
    const auto& path = hashToPath(hash);
    return path;
}

String Manager::stringToHash(const String& s)
{
    WTF::MD5 md5;
    if (s.characters8())
        md5.addBytes(static_cast<const uint8_t*>(s.characters8()), s.length());
    else
        md5.addBytes(reinterpret_cast<const uint8_t*>(s.characters16()), 2 * s.length());

    WTF::MD5::Digest digest;
    md5.checksum(digest);

    return WTF::base64URLEncode(&digest[0], WTF::MD5::hashSize);
}

String Manager::hashToPath(const String& hash)
{
    auto hashHead = hash.substring(0, 2);
    auto hashTail = hash.substring(2);

    StringBuilder fileName;
    fileName.append(hashTail);
    fileName.appendLiteral(".data");

    auto path = pathByAppendingComponent(m_recordReplayCacheLocation, kDirNameResources);
    path = pathByAppendingComponent(path, hashHead);
    path = pathByAppendingComponent(path, fileName.toString());

    return path;
}

String Manager::urlIdentifyingCommonDomain(const URL& url)
{
    return url.protocolHostAndPort();
}

void Manager::logRecordedResource(const WebCore::ResourceRequest& request)
{
    // Log network resources as they are cached to disk.

    const auto& url = request.url();
    m_recordFileHandle.printf("%s %s\n", DEBUG_STR(stringToHash(url.string())), DEBUG_STR(url.string()));
}

void Manager::logLoadedResource(Resource& resource)
{
    // Log cached resources as they are loaded from disk.

    m_loadFileHandle.printf("%s\n", DEBUG_STR(resource.url().string()));
}

void Manager::logPlayedBackResource(const WebCore::ResourceRequest& request, bool wasCacheMiss)
{
    // Log network resources that are requested during replay.

    const auto& url = request.url();

    if (wasCacheMiss)
        DEBUG_LOG("Cache miss: URL = " STRING_SPECIFIER, DEBUG_STR(url.string()));
    else
        DEBUG_LOG("Cache hit:  URL = " STRING_SPECIFIER, DEBUG_STR(url.string()));

    m_replayFileHandle.printf("%s %s\n", wasCacheMiss ? "miss" : "hit ", DEBUG_STR(url.string()));
}

WebCore::FileHandle Manager::openCacheFile(const String& filePath, FileOpenMode mode)
{
    // If we can trivially open the file, then do that and return the new file
    // handle.

    auto fileHandle = WebCore::FileHandle(filePath, mode);
    if (fileHandle.open())
        return fileHandle;

    // If we're opening the file for writing (including appending), then try
    // again after making sure all intermediate directories have been created.

    if (mode != FileOpenMode::Read) {
        const auto& parentDir = directoryName(filePath);
        if (!makeAllDirectories(parentDir)) {
            DEBUG_LOG_ERROR("Error %d trying to create intermediate directories: " STRING_SPECIFIER, errno, DEBUG_STR(parentDir));
            return fileHandle;
        }

        fileHandle = WebCore::FileHandle(filePath, mode);
        if (fileHandle.open())
            return fileHandle;
    }

    // Could not open the file. Log the error and leave, returning the invalid
    // file handle.

    if (mode == FileOpenMode::Read)
        DEBUG_LOG_ERROR("Error %d trying to open " STRING_SPECIFIER " for reading", errno, DEBUG_STR(filePath));
    else
        DEBUG_LOG_ERROR("Error %d trying to open " STRING_SPECIFIER " for writing", errno, DEBUG_STR(filePath));

    return fileHandle;
}

std::optional<Vector<Vector<String>>> Manager::readFile(const String& filePath)
{
    bool success = false;
    MappedFileData file(filePath, success);
    if (!success)
        return std::nullopt;

    Vector<Vector<String>> lines;
    auto begin = static_cast<const uint8_t*>(file.data());
    auto end = begin + file.size();

    Vector<String> line;
    while (getLine(begin, end, line))
        lines.append(WTFMove(line));

    return WTFMove(lines);
}

bool Manager::getLine(uint8_t const *& p, uint8_t const * const end, Vector<String>& line)
{
    // NB: Returns true if there may be more data to get, false if we've hit
    // the end of the buffer.

    DEBUG_LOG_VERBOSE("Getting a line");

    line.clear();

    if (p == end) {
        DEBUG_LOG_VERBOSE("Iterator at end; returning false");
        return false;
    }

    String word;
    while (getWord(p, end, word)) {
        if (!word.isEmpty()) {
            DEBUG_LOG_VERBOSE("Adding word: " STRING_SPECIFIER, DEBUG_STR(word));
            line.append(word);
        }
    }

    return true;
}

bool Manager::getWord(uint8_t const *& p, uint8_t const * const end, String& word)
{
    // NB: Returns true if a (possibly empty) word was found and there may be
    // more, false if we've hit the end of line or buffer.

    DEBUG_LOG_VERBOSE("Getting a word");

    if (p == end) {
        DEBUG_LOG_VERBOSE("Iterator at end; returning false");
        return false;
    }

    if (*p == '\n') {
        DEBUG_LOG_VERBOSE("Iterator hit EOL; returning false");
        ++p;
        return false;
    }

    bool escaping = false;
    bool ignoring = false;

    word = String();

    DEBUG_LOG_VERBOSE("Iterating");

    for ( ; p != end; ++p) {
        if (ignoring) {
            if (*p == '\n')
                break;
        } else if (escaping) {
            word.append(*p);
            escaping = false;
        } else if (*p == '#') {
            ignoring = true;
        } else if (*p == '\\') {
            escaping = true;
        } else if (*p == ' ') {
            if (!word.isEmpty())
                break;
        } else if (*p == '\n')
            break;
        else
            word.append(*p);
    }

    return true;
}

} // namespace NetworkCapture
} // namespace WebKit

#undef DEBUG_CLASS

#endif // ENABLE(NETWORK_CAPTURE)
