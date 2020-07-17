/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef DownloadID_h
#define DownloadID_h

#include "ArgumentCoder.h"
#include "Decoder.h"
#include "Encoder.h"
#include <wtf/HashTraits.h>

namespace WebKit {

class DownloadID {
public:
    DownloadID()
    {
    }

    explicit DownloadID(uint64_t downloadID)
        : m_downloadID(downloadID)
    {
    }

    bool operator==(DownloadID other) const { return m_downloadID == other.m_downloadID; }
    bool operator!=(DownloadID other) const { return m_downloadID != other.m_downloadID; }

    explicit operator bool() const { return downloadID(); }

    uint64_t downloadID() const { return m_downloadID; }
private:
    uint64_t m_downloadID { 0 };
};

}

namespace IPC {
    
template<> struct ArgumentCoder<WebKit::DownloadID> {
    static void encode(Encoder& encoder, const WebKit::DownloadID& downloadID)
    {
        encoder << downloadID.downloadID();
    }
    static WARN_UNUSED_RETURN bool decode(Decoder& decoder, WebKit::DownloadID& downloadID)
    {
        uint64_t id;
        if (!decoder.decode(id))
            return false;

        downloadID = WebKit::DownloadID(id);
        
        return true;
    }
};

}

namespace WTF {
    
struct DownloadIDHash {
    static unsigned hash(const WebKit::DownloadID& d) { return intHash(d.downloadID()); }
    static bool equal(const WebKit::DownloadID& a, const WebKit::DownloadID& b) { return a.downloadID() == b.downloadID(); }
    static const bool safeToCompareToEmptyOrDeleted = true;
};
template<> struct HashTraits<WebKit::DownloadID> : GenericHashTraits<WebKit::DownloadID> {
    static WebKit::DownloadID emptyValue() { return { }; }
    
    static void constructDeletedValue(WebKit::DownloadID& slot) { slot = WebKit::DownloadID(std::numeric_limits<uint64_t>::max()); }
    static bool isDeletedValue(const WebKit::DownloadID& slot) { return slot.downloadID() == std::numeric_limits<uint64_t>::max(); }
};
template<> struct DefaultHash<WebKit::DownloadID> : DownloadIDHash { };

}
#endif /* DownloadID_h */
