/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef PlugInAutoStartProvider_h
#define PlugInAutoStartProvider_h

#include <pal/SessionID.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>
#include <wtf/WallTime.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace API {
class Array;
class Dictionary;
}

namespace WebKit {

class WebProcessPool;

typedef HashMap<unsigned, WallTime> PlugInAutoStartOriginMap;
typedef HashMap<PAL::SessionID, PlugInAutoStartOriginMap> SessionPlugInAutoStartOriginMap;
typedef Vector<String> PlugInAutoStartOrigins;

class PlugInAutoStartProvider {
    WTF_MAKE_NONCOPYABLE(PlugInAutoStartProvider);
public:
    explicit PlugInAutoStartProvider(WebProcessPool*);

    void addAutoStartOriginHash(const String& pageOrigin, unsigned plugInOriginHash, PAL::SessionID);
    void didReceiveUserInteraction(unsigned plugInOriginHash, PAL::SessionID);

    Ref<API::Dictionary> autoStartOriginsTableCopy() const;
    void setAutoStartOriginsTable(API::Dictionary&);
    void setAutoStartOriginsFilteringOutEntriesAddedAfterTime(API::Dictionary&, WallTime);
    void setAutoStartOriginsArray(API::Array&);

    PlugInAutoStartOriginMap autoStartOriginHashesCopy(PAL::SessionID) const;
    const PlugInAutoStartOrigins& autoStartOrigins() const { return m_autoStartOrigins; }

    using HashToOriginMap = HashMap<unsigned, String>;
    using PerSessionHashToOriginMap = HashMap<PAL::SessionID, HashToOriginMap>;
    using AutoStartTable = HashMap<String, PlugInAutoStartOriginMap, ASCIICaseInsensitiveHash>;

private:
    WebProcessPool* m_processPool;

    void setAutoStartOriginsTableWithItemsPassingTest(API::Dictionary&, WTF::Function<bool(WallTime expirationTimestamp)>&&);

    typedef HashMap<PAL::SessionID, AutoStartTable> SessionAutoStartTable;
    SessionAutoStartTable m_autoStartTable;

    PerSessionHashToOriginMap m_hashToOriginMap;

    PlugInAutoStartOrigins m_autoStartOrigins;
};

} // namespace WebKit

#endif /* PlugInAutoStartProvider_h */
