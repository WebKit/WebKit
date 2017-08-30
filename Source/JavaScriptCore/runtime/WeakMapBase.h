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

#pragma once

#include "JSDestructibleObject.h"
#include "Structure.h"
#include <wtf/HashMap.h>
#include <wtf/MathExtras.h>

namespace JSC {

class WeakMapBase : public JSDestructibleObject {
public:
    using Base = JSDestructibleObject;

    void set(VM&, JSObject*, JSValue);
    JSValue get(JSObject*);
    bool remove(JSObject*);
    bool contains(JSObject*);
    void clear();

    DECLARE_INFO;

    using MapType = HashMap<JSObject*, WriteBarrier<Unknown>>;
    MapType::const_iterator begin() const { return m_map.begin(); }
    MapType::const_iterator end() const { return m_map.end(); }

    unsigned size() const { return m_map.size(); }

    static size_t estimatedSize(JSCell*);
    static void visitChildren(JSCell*, SlotVisitor&);

protected:
    WeakMapBase(VM&, Structure*);
    static void destroy(JSCell*);

    class DeadKeyCleaner : public UnconditionalFinalizer, public WeakReferenceHarvester {
    public:
        WeakMapBase* target();

    private:
        void visitWeakReferences(SlotVisitor&) override;
        void finalizeUnconditionally() override;
        unsigned m_liveKeyCount;
    };
    DeadKeyCleaner m_deadKeyCleaner;
    MapType m_map;
};

} // namespace JSC
