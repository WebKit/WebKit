/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Samuel Weinig <sam@webkit.org>
 *  Copyright (C) 2009 Google, Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef DOMWrapperWorld_h
#define DOMWrapperWorld_h

#include "Document.h"
#include "JSDOMGlobalObject.h"
#include "JSDOMWrapper.h"
#include <runtime/WeakGCMap.h>

namespace WebCore {

class ScriptController;
class StringImpl;

typedef JSC::WeakGCMap<void*, DOMObject*> DOMObjectWrapperMap;
typedef JSC::WeakGCMap<StringImpl*, JSC::JSString*> JSStringCache; 

class DOMWrapperWorld : public RefCounted<DOMWrapperWorld> {
public:
    static PassRefPtr<DOMWrapperWorld> create(JSC::JSGlobalData* globalData, bool isNormal = false)
    {
        return adoptRef(new DOMWrapperWorld(globalData, isNormal));
    }
    ~DOMWrapperWorld();
    
    void registerWorld();
    void unregisterWorld();

    void didCreateWrapperCache(Document* document) { m_documentsWithWrapperCaches.add(document); }
    void didDestroyWrapperCache(Document* document) { m_documentsWithWrapperCaches.remove(document); }

    void didCreateWindowShell(ScriptController* scriptController) { m_scriptControllersWithWindowShells.add(scriptController); }
    void didDestroyWindowShell(ScriptController* scriptController) { m_scriptControllersWithWindowShells.remove(scriptController); }

    // FIXME: can we make this private?
    DOMObjectWrapperMap m_wrappers;
    JSStringCache m_stringCache;

    bool isNormal() const { return m_isNormal; }

protected:
    DOMWrapperWorld(JSC::JSGlobalData*, bool isNormal);

private:
    JSC::JSGlobalData* m_globalData;
    HashSet<Document*> m_documentsWithWrapperCaches;
    HashSet<ScriptController*> m_scriptControllersWithWindowShells;
    bool m_isNormal;
    bool m_isRegistered;
};

DOMWrapperWorld* normalWorld(JSC::JSGlobalData&);
DOMWrapperWorld* mainThreadNormalWorld();
inline DOMWrapperWorld* debuggerWorld() { return mainThreadNormalWorld(); }
inline DOMWrapperWorld* pluginWorld() { return mainThreadNormalWorld(); }

inline DOMWrapperWorld* currentWorld(JSC::ExecState* exec)
{
    return static_cast<JSDOMGlobalObject*>(exec->lexicalGlobalObject())->world();
}

// From Document.h

inline Document::JSWrapperCache* Document::getWrapperCache(DOMWrapperWorld* world)
{
    if (world->isNormal()) {
        if (Document::JSWrapperCache* wrapperCache = m_normalWorldWrapperCache)
            return wrapperCache;
        ASSERT(!m_wrapperCacheMap.contains(world));
    } else if (Document::JSWrapperCache* wrapperCache = m_wrapperCacheMap.get(world))
        return wrapperCache;
    return createWrapperCache(world);
}

} // namespace WebCore

#endif // DOMWrapperWorld_h
