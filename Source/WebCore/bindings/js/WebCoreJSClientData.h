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
    
    JSC::IsoSubspace& runtimeMethodSpace() { return m_runtimeMethodSpace; }
    
    JSC::CompleteSubspace& outputConstraintSpace() { return m_outputConstraintSpace; }
    JSC::CompleteSubspace& globalObjectOutputConstraintSpace() { return m_globalObjectOutputConstraintSpace; }
    
    template<typename Func>
    void forEachOutputConstraintSpace(const Func& func)
    {
        func(m_outputConstraintSpace);
        func(m_globalObjectOutputConstraintSpace);
    }

private:
    HashSet<DOMWrapperWorld*> m_worldSet;
    RefPtr<DOMWrapperWorld> m_normalWorld;

    JSBuiltinFunctions m_builtinFunctions;
    WebCoreBuiltinNames m_builtinNames;
    
    JSC::IsoSubspace m_runtimeMethodSpace;
    
    JSC::CompleteSubspace m_outputConstraintSpace;
    JSC::CompleteSubspace m_globalObjectOutputConstraintSpace;
};

} // namespace WebCore
