/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2021 Apple Inc. All rights reserved.
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

#pragma once

#include "DOMWrapperWorld.h"
#include "WebCoreBuiltinNames.h"
#include "WebCoreJSBuiltins.h"
#include "WorkerThreadType.h"
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class DOMIsoSubspaces;

class JSVMClientData : public JSC::VM::ClientData {
    WTF_MAKE_NONCOPYABLE(JSVMClientData); WTF_MAKE_FAST_ALLOCATED;
    friend class VMWorldIterator;

public:
    explicit JSVMClientData(JSC::VM&);

    virtual ~JSVMClientData();
    
    WEBCORE_EXPORT static void initNormalWorld(JSC::VM*, WorkerThreadType);

    DOMWrapperWorld& normalWorld() { return *m_normalWorld; }

    void getAllWorlds(Vector<Ref<DOMWrapperWorld>>&);

    void rememberWorld(DOMWrapperWorld& world)
    {
        ASSERT(!m_worldSet.contains(&world));
        m_worldSet.add(&world);
    }

    void forgetWorld(DOMWrapperWorld& world)
    {
        ASSERT(m_worldSet.contains(&world));
        m_worldSet.remove(&world);
    }

    WebCoreBuiltinNames& builtinNames() { return m_builtinNames; }
    JSBuiltinFunctions& builtinFunctions() { return m_builtinFunctions; }
    
    JSC::IsoSubspace& domBuiltinConstructorSpace() { return m_domBuiltinConstructorSpace; }
    JSC::IsoSubspace& domConstructorSpace() { return m_domConstructorSpace; }
    JSC::IsoSubspace& domNamespaceObjectSpace() { return m_domNamespaceObjectSpace; }
    JSC::IsoSubspace& domWindowPropertiesSpace() { return m_domWindowPropertiesSpace; }
    JSC::IsoSubspace& runtimeArraySpace() { return m_runtimeArraySpace; }
    JSC::IsoSubspace& runtimeMethodSpace() { return m_runtimeMethodSpace; }
    JSC::IsoSubspace& runtimeObjectSpace() { return m_runtimeObjectSpace; }
    JSC::IsoSubspace& windowProxySpace() { return m_windowProxySpace; }
    JSC::IsoSubspace& idbSerializationSpace() { return m_idbSerializationSpace; }
    JSC::IsoSubspace* fileSystemDirectoryHandleIteratorSpace() { return m_fileSystemDirectoryHandleIteratorSpace.get(); }
    void setFileSystemDirectoryHandleIteratorSpace(std::unique_ptr<JSC::IsoSubspace> space) { m_fileSystemDirectoryHandleIteratorSpace = std::exchange(space, nullptr); }

    Vector<JSC::IsoSubspace*>& outputConstraintSpaces() { return m_outputConstraintSpaces; }

    template<typename Func>
    void forEachOutputConstraintSpace(const Func& func)
    {
        for (auto* space : m_outputConstraintSpaces)
            func(*space);
    }

    DOMIsoSubspaces& subspaces() { return *m_subspaces.get(); }

private:
    HashSet<DOMWrapperWorld*> m_worldSet;
    RefPtr<DOMWrapperWorld> m_normalWorld;

    JSBuiltinFunctions m_builtinFunctions;
    WebCoreBuiltinNames m_builtinNames;

    JSC::IsoHeapCellType m_runtimeArrayHeapCellType;
    JSC::IsoHeapCellType m_runtimeObjectHeapCellType;
    JSC::IsoHeapCellType m_windowProxyHeapCellType;
public:
    JSC::IsoHeapCellType m_heapCellTypeForJSDOMWindow;
    JSC::IsoHeapCellType m_heapCellTypeForJSDedicatedWorkerGlobalScope;
    JSC::IsoHeapCellType m_heapCellTypeForJSRemoteDOMWindow;
    JSC::IsoHeapCellType m_heapCellTypeForJSWorkerGlobalScope;
    JSC::IsoHeapCellType m_heapCellTypeForJSSharedWorkerGlobalScope;
#if ENABLE(SERVICE_WORKER)
    JSC::IsoHeapCellType m_heapCellTypeForJSServiceWorkerGlobalScope;
#endif
    JSC::IsoHeapCellType m_heapCellTypeForJSWorkletGlobalScope;
#if ENABLE(CSS_PAINTING_API)
    JSC::IsoHeapCellType m_heapCellTypeForJSPaintWorkletGlobalScope;
#endif
#if ENABLE(WEB_AUDIO)
    JSC::IsoHeapCellType m_heapCellTypeForJSAudioWorkletGlobalScope;
#endif
    JSC::IsoHeapCellType m_heapCellTypeForJSIDBSerializationGlobalObject;

private:
    JSC::IsoSubspace m_domBuiltinConstructorSpace;
    JSC::IsoSubspace m_domConstructorSpace;
    JSC::IsoSubspace m_domNamespaceObjectSpace;
    JSC::IsoSubspace m_domWindowPropertiesSpace;
    JSC::IsoSubspace m_runtimeArraySpace;
    JSC::IsoSubspace m_runtimeMethodSpace;
    JSC::IsoSubspace m_runtimeObjectSpace;
    JSC::IsoSubspace m_windowProxySpace;
    JSC::IsoSubspace m_idbSerializationSpace;
    std::unique_ptr<JSC::IsoSubspace> m_fileSystemDirectoryHandleIteratorSpace;
    std::unique_ptr<DOMIsoSubspaces> m_subspaces;
    Vector<JSC::IsoSubspace*> m_outputConstraintSpaces;
};

} // namespace WebCore
