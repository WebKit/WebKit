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

#include "config.h"
#include "PlugInAutoStartProvider.h"

#include "WebContext.h"
#include "WebContextClient.h"
#include "WebProcessMessages.h"

using namespace WebCore;

namespace WebKit {

PlugInAutoStartProvider::PlugInAutoStartProvider(WebContext* context)
    : m_context(context)
{
}

void PlugInAutoStartProvider::addAutoStartOrigin(const String& pageOrigin, unsigned plugInOriginHash)
{
    if (m_autoStartHashes.contains(plugInOriginHash))
        return;
    
    AutoStartTable::iterator it = m_autoStartTable.find(pageOrigin);
    if (it == m_autoStartTable.end())
        it = m_autoStartTable.add(pageOrigin, HashSet<unsigned>()).iterator;
    
    it->value.add(plugInOriginHash);
    m_autoStartHashes.add(plugInOriginHash);
    m_context->sendToAllProcesses(Messages::WebProcess::DidAddPlugInAutoStartOrigin(plugInOriginHash));
    m_context->client().plugInAutoStartOriginHashesChanged(m_context);
}

Vector<unsigned> PlugInAutoStartProvider::autoStartOriginsCopy() const
{
    Vector<unsigned> copyVector;
    copyToVector(m_autoStartHashes, copyVector);
    return copyVector;
}

PassRefPtr<ImmutableDictionary> PlugInAutoStartProvider::autoStartOriginsTableCopy() const
{
    ImmutableDictionary::MapType map;
    AutoStartTable::const_iterator end = m_autoStartTable.end();
    for (AutoStartTable::const_iterator it = m_autoStartTable.begin(); it != end; ++it) {
        Vector<RefPtr<APIObject> > hashes;
        HashSet<unsigned>::iterator valueEnd = it->value.end();
        for (HashSet<unsigned>::iterator valueIt = it->value.begin(); valueIt != valueEnd; ++valueIt)
            hashes.append(WebUInt64::create(*valueIt));

        map.set(it->key, ImmutableArray::adopt(hashes));
    }

    return ImmutableDictionary::adopt(map);
}

} // namespace WebKit
