/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef WeakMapData_h
#define WeakMapData_h

#include "JSCell.h"
#include "Structure.h"
#include <wtf/HashFunctions.h>
#include <wtf/HashMap.h>
#include <wtf/MathExtras.h>

namespace JSC {

class WeakMapData : public JSCell {
public:
    typedef JSCell Base;

    static WeakMapData* create(VM& vm)
    {
        WeakMapData* weakMapData = new (NotNull, allocateCell<WeakMapData>(vm.heap)) WeakMapData(vm);
        weakMapData->finishCreation(vm);
        return weakMapData;
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(CompoundType, StructureFlags), info());
    }

    static const bool needsDestruction = true;
    static const bool hasImmortalStructure = true;

    void set(VM&, JSObject*, JSValue);
    JSValue get(JSObject*);
    bool remove(JSObject*);
    bool contains(JSObject*);
    void clear();

    DECLARE_INFO;

    static const unsigned StructureFlags = OverridesVisitChildren | Base::StructureFlags;

private:
    WeakMapData(VM&);
    static void destroy(JSCell*);
    static void visitChildren(JSCell*, SlotVisitor&);
    void finishCreation(VM&);

    class DeadKeyCleaner : public UnconditionalFinalizer, public WeakReferenceHarvester {
    public:
        DeadKeyCleaner(WeakMapData* target)
            : m_target(target)
        {
        }
    private:
        virtual void visitWeakReferences(SlotVisitor&) override;
        virtual void finalizeUnconditionally() override;
        int m_liveKeyCount;
        WeakMapData* m_target;
    };
    DeadKeyCleaner m_deadKeyCleaner;
    typedef HashMap<JSObject*, WriteBarrier<Unknown>> MapType;
    MapType m_map;
};

}

#endif /* !defined(WeakMapData_h) */
