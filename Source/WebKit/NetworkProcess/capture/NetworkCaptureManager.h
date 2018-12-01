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

#pragma once

#if ENABLE(NETWORK_CAPTURE)

#include <WebCore/FileHandle.h>
#include <WebCore/FileSystem.h>
#include <wtf/Function.h>
#include <wtf/URLParser.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class ResourceRequest;
}

namespace WebKit {
namespace NetworkCapture {

class Resource;

/*
 * NetworkCapture::Manager serves three purposes:
 *
 *  * It keeps the state of whether we are recording, replaying, or neither.
 *  * It keeps the list of cached resources (if replaying), performs fuzzy
 *    matching on them.
 *  * It has utilities for logging and file management.
 *
 * TODO: Perhaps we should break this up into three classes?
 */
class Manager {
    WTF_MAKE_NONCOPYABLE(Manager);
    friend NeverDestroyed<Manager>;

public:
    enum RecordReplayMode {
        Disabled,
        Record,
        Replay
    };

    static Manager& singleton();

    void initialize(const String& recordReplayMode, const String& recordReplayCacheLocation);
    void terminate();

    bool isRecording() const { return mode() == RecordReplayMode::Record; }
    bool isReplaying() const { return mode() == RecordReplayMode::Replay; }
    RecordReplayMode mode() const { return m_recordReplayMode; }

    Resource* findMatch(const WebCore::ResourceRequest&);

    void logRecordedResource(const WebCore::ResourceRequest&);
    void logLoadedResource(Resource&);
    void logPlayedBackResource(const WebCore::ResourceRequest&, bool wasCacheMiss);

    WebCore::FileHandle openCacheFile(const String&, WebCore::FileSystem::FileOpenMode);

    String requestToPath(const WebCore::ResourceRequest&);
    static String urlIdentifyingCommonDomain(const URL&);

private:
    Manager() = default;
    ~Manager() = delete;

    Resource* findExactMatch(const WebCore::ResourceRequest&);
    Resource* findBestFuzzyMatch(const WebCore::ResourceRequest&);
    int fuzzyMatchURLs(const URL& requestURL, const WTF::URLParser::URLEncodedForm& requestParameters, const URL& resourceURL, const WTF::URLParser::URLEncodedForm& resourceParameters);

    void loadResources();

    String reportLoadPath();
    String reportRecordPath();
    String reportReplayPath();

    String stringToHash(const String&);
    String hashToPath(const String& hash);

    std::optional<Vector<Vector<String>>> readFile(const String& filePath);
    bool getLine(uint8_t const *& p, uint8_t const * const end, Vector<String>& line);
    bool getWord(uint8_t const *& p, uint8_t const * const end, String& word);

    RecordReplayMode m_recordReplayMode { Disabled };
    String m_recordReplayCacheLocation;

    WebCore::FileHandle m_loadFileHandle;
    WebCore::FileHandle m_recordFileHandle;
    WebCore::FileHandle m_replayFileHandle;

    Vector<Resource> m_cachedResources;
};

} // namespace NetworkCapture
} // namespace WebKit

#endif // ENABLE(NETWORK_CAPTURE)
