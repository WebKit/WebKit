/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2017 Apple Inc. All rights reserved.
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
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class JSVMClientData : public JSC::VM::ClientData {
    WTF_MAKE_NONCOPYABLE(JSVMClientData); WTF_MAKE_FAST_ALLOCATED;
    friend class VMWorldIterator;

public:
    explicit JSVMClientData(JSC::VM&);

    virtual ~JSVMClientData();
    
    WEBCORE_EXPORT static void initNormalWorld(JSC::VM*);

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
    JSC::IsoSubspace& domWindowPropertiesSpace() { return m_domWindowPropertiesSpace; }
    JSC::IsoSubspace& runtimeArraySpace() { return m_runtimeArraySpace; }
    JSC::IsoSubspace& runtimeMethodSpace() { return m_runtimeMethodSpace; }
    JSC::IsoSubspace& runtimeObjectSpace() { return m_runtimeObjectSpace; }
    JSC::IsoSubspace& windowProxySpace() { return m_windowProxySpace; }
    
    JSC::CompleteSubspace& outputConstraintSpace() { return m_outputConstraintSpace; }

    JSC::IsoSubspace& subspaceForJSDOMWindow() { return m_subspaceForJSDOMWindow; }
    JSC::IsoSubspace& subspaceForJSDedicatedWorkerGlobalScope() { return m_subspaceForJSDedicatedWorkerGlobalScope; }
    JSC::IsoSubspace& subspaceForJSRemoteDOMWindow() { return m_subspaceForJSRemoteDOMWindow; }
    JSC::IsoSubspace& subspaceForJSWorkerGlobalScope() { return m_subspaceForJSWorkerGlobalScope; }
#if ENABLE(SERVICE_WORKER)
    JSC::IsoSubspace& subspaceForJSServiceWorkerGlobalScope() { return m_subspaceForJSServiceWorkerGlobalScope; }
#endif
#if ENABLE(CSS_PAINTING_API)
    JSC::IsoSubspace& subspaceForJSPaintWorkletGlobalScope() { return m_subspaceForJSPaintWorkletGlobalScope; }
    JSC::IsoSubspace& subspaceForJSWorkletGlobalScope() { return m_subspaceForJSWorkletGlobalScope; }
#endif
    
    template<typename Func>
    void forEachOutputConstraintSpace(const Func& func)
    {
        func(m_outputConstraintSpace);
        func(m_subspaceForJSDOMWindow);
        func(m_subspaceForJSDedicatedWorkerGlobalScope);
        func(m_subspaceForJSRemoteDOMWindow);
        func(m_subspaceForJSWorkerGlobalScope);
#if ENABLE(SERVICE_WORKER)
        func(m_subspaceForJSServiceWorkerGlobalScope);
#endif
#if ENABLE(CSS_PAINTING_API)
        func(m_subspaceForJSPaintWorkletGlobalScope);
        func(m_subspaceForJSWorkletGlobalScope);
#endif
    }

private:
    HashSet<DOMWrapperWorld*> m_worldSet;
    RefPtr<DOMWrapperWorld> m_normalWorld;

    JSBuiltinFunctions m_builtinFunctions;
    WebCoreBuiltinNames m_builtinNames;

    std::unique_ptr<JSC::HeapCellType> m_runtimeArrayHeapCellType;
    std::unique_ptr<JSC::HeapCellType> m_runtimeObjectHeapCellType;
    std::unique_ptr<JSC::HeapCellType> m_windowProxyHeapCellType;

    std::unique_ptr<JSC::HeapCellType> m_heapCellTypeForJSDOMWindow;
    std::unique_ptr<JSC::HeapCellType> m_heapCellTypeForJSDedicatedWorkerGlobalScope;
    std::unique_ptr<JSC::HeapCellType> m_heapCellTypeForJSRemoteDOMWindow;
    std::unique_ptr<JSC::HeapCellType> m_heapCellTypeForJSWorkerGlobalScope;
#if ENABLE(SERVICE_WORKER)
    std::unique_ptr<JSC::HeapCellType> m_heapCellTypeForJSServiceWorkerGlobalScope;
#endif
#if ENABLE(CSS_PAINTING_API)
    std::unique_ptr<JSC::HeapCellType> m_heapCellTypeForJSPaintWorkletGlobalScope;
    std::unique_ptr<JSC::HeapCellType> m_heapCellTypeForJSWorkletGlobalScope;
#endif

    JSC::IsoSubspace m_domBuiltinConstructorSpace;
    JSC::IsoSubspace m_domConstructorSpace;
    JSC::IsoSubspace m_domWindowPropertiesSpace;
    JSC::IsoSubspace m_runtimeArraySpace;
    JSC::IsoSubspace m_runtimeMethodSpace;
    JSC::IsoSubspace m_runtimeObjectSpace;
    JSC::IsoSubspace m_windowProxySpace;

    JSC::IsoSubspace m_subspaceForJSDOMWindow;
    JSC::IsoSubspace m_subspaceForJSDedicatedWorkerGlobalScope;
    JSC::IsoSubspace m_subspaceForJSRemoteDOMWindow;
    JSC::IsoSubspace m_subspaceForJSWorkerGlobalScope;
#if ENABLE(SERVICE_WORKER)
    JSC::IsoSubspace m_subspaceForJSServiceWorkerGlobalScope;
#endif
#if ENABLE(CSS_PAINTING_API)
    JSC::IsoSubspace m_subspaceForJSPaintWorkletGlobalScope;
    JSC::IsoSubspace m_subspaceForJSWorkletGlobalScope;
#endif
    
    JSC::CompleteSubspace m_outputConstraintSpace;
};

} // namespace WebCore
