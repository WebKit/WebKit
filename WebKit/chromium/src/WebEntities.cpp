/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebEntities.h"

#include <string.h>

#include "HTMLEntityTable.h"
#include "PlatformString.h"
#include "StringBuilder.h"
#include <wtf/HashMap.h>

#include "WebString.h"

using namespace WebCore;

namespace WebKit {

namespace {

void populateMapFromXMLEntities(WTF::HashMap<int, WTF::String>& map)
{
    ASSERT(map.isEmpty());
    map.set(0x003c, "lt");
    map.set(0x003e, "gt");
    map.set(0x0026, "amp");
    map.set(0x0027, "apos");
    map.set(0x0022, "quot");
}

void populateMapFromHTMLEntityTable(WTF::HashMap<int, WTF::String>& map)
{
    ASSERT(map.isEmpty());
    const HTMLEntityTableEntry* entry = HTMLEntityTable::firstEntry();
    const HTMLEntityTableEntry* end = HTMLEntityTable::lastEntry() + 1;
    for (; entry != end; ++entry) {
        String entity = entry->entity;
        int value = entry->value;
        ASSERT(value && !entity.isEmpty());
        if (entity[entity.length() - 1] != ';')
            continue; // We want the canonical version that ends in ;
        // For consistency, use the lower case for entities that have both.
        if (map.contains(value) && map.get(value) == entity.lower())
            continue;
        // Don't register &percnt;, &nsup; and &supl; for some unknown reason.
        if (value == '%' || value == 0x2285 || value == 0x00b9)
            continue;
        map.set(value, entity);
    }
    // We add #39 for some unknown reason.
    map.set(0x0027, String("#39"));
}

}

WebEntities::WebEntities(bool xmlEntities)
{
    if (xmlEntities)
        populateMapFromXMLEntities(m_entitiesMap);
    else
        populateMapFromHTMLEntityTable(m_entitiesMap);
}

String WebEntities::entityNameByCode(int code) const
{
    if (m_entitiesMap.contains(code))
        return m_entitiesMap.get(code);
    return "";
}

String WebEntities::convertEntitiesInString(const String& value) const
{
    unsigned len = value.length();
    const UChar* startPos = value.characters();
    const UChar* curPos = startPos;

    // FIXME: Optimize - create StringBuilder only if value has any entities.
    StringBuilder result;
    while (len--) {
        if (m_entitiesMap.contains(*curPos)) {
            // Append content before entity code.
            if (curPos > startPos)
                result.append(String(startPos, curPos - startPos));
            result.append("&");
            result.append(m_entitiesMap.get(*curPos));
            result.append(";");
            startPos = ++curPos;
        } else
            curPos++;
    }
    // Append the remaining content.
    if (curPos > startPos)
        result.append(String(startPos, curPos - startPos));

    return result.toString();
}

} // namespace WebKit
