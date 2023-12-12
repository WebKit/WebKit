/*
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TypeProfilerLog.h"

#include "FrameTracers.h"
#include "JSCJSValueInlines.h"
#include "TypeLocation.h"
#include <wtf/TZoneMallocInlines.h>

namespace JSC {

namespace TypeProfilerLogInternal {
static constexpr bool verbose = false;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(TypeProfilerLog);

TypeProfilerLog::TypeProfilerLog(VM& vm)
    : m_vm(vm)
    , m_logSize(50000)
    , m_logStartPtr(new LogEntry[m_logSize])
    , m_currentLogEntryPtr(m_logStartPtr)
    , m_logEndPtr(m_logStartPtr + m_logSize)
{
    ASSERT(m_logStartPtr);
}

TypeProfilerLog::~TypeProfilerLog()
{
    delete[] m_logStartPtr;
}

void TypeProfilerLog::processLogEntries(VM& vm, const String& reason)
{
    // We need to do this because this code will call into calculatedDisplayName.
    // calculatedDisplayName will clear any exception it sees (because it thinks
    // it's a stack overflow). We may be called when an exception was already
    // thrown, so we don't want calculatedDisplayName to clear that exception that
    // was thrown before we even got here.
    SuspendExceptionScope suspendExceptionScope(vm);

    MonotonicTime before { };
    if (TypeProfilerLogInternal::verbose) {
        dataLog("Process caller:'", reason, "'");
        before = MonotonicTime::now();
    }

    HashMap<Structure*, RefPtr<StructureShape>> cachedMonoProtoShapes;
    HashMap<std::pair<Structure*, JSCell*>, RefPtr<StructureShape>> cachedPolyProtoShapes;

    LogEntry* entry = m_logStartPtr;

    while (entry != m_currentLogEntryPtr) {
        StructureID id = entry->structureID;
        RefPtr<StructureShape> shape;
        JSValue value = entry->value;
        Structure* structure = nullptr;
        bool sawPolyProtoStructure = false;
        if (id) {
            structure = id.decode();
            auto iter = cachedMonoProtoShapes.find(structure);
            if (iter == cachedMonoProtoShapes.end()) {
                auto key = std::make_pair(structure, value.asCell());
                auto iter = cachedPolyProtoShapes.find(key);
                if (iter != cachedPolyProtoShapes.end()) {
                    shape = iter->value;
                    sawPolyProtoStructure = true;
                }

                if (!shape) {
                    shape = structure->toStructureShape(value, sawPolyProtoStructure);
                    if (sawPolyProtoStructure)
                        cachedPolyProtoShapes.set(key, shape);
                    else
                        cachedMonoProtoShapes.set(structure, shape);
                }
            } else
                shape = iter->value;
        }

        RuntimeType type = runtimeTypeForValue(value);
        TypeLocation* location = entry->location;
        location->m_lastSeenType = type;
        if (location->m_globalTypeSet)
            location->m_globalTypeSet->addTypeInformation(type, shape.copyRef(), structure, sawPolyProtoStructure);
        location->m_instructionTypeSet->addTypeInformation(type, WTFMove(shape), structure, sawPolyProtoStructure);

        entry++;
    }

    // Note that we don't update this cursor until we're done processing the log.
    // This allows us to have a sane story in case we have to mark the log
    // while processing through it. We won't be iterating over the log while
    // marking it, but we may be in the middle of iterating over when the mutator
    // pauses and causes the collector to mark the log.
    m_currentLogEntryPtr = m_logStartPtr;

    if (TypeProfilerLogInternal::verbose) {
        MonotonicTime after = MonotonicTime::now();
        dataLogF(" Processing the log took: '%f' ms\n", (after - before).milliseconds());
    }
}

// We don't need a SlotVisitor version of this because TypeProfilerLog is only used by
// dev tools, and is therefore not on the critical path for performance.
void TypeProfilerLog::visit(AbstractSlotVisitor& visitor)
{
    for (LogEntry* entry = m_logStartPtr; entry != m_currentLogEntryPtr; ++entry) {
        visitor.appendUnbarriered(entry->value);
        if (StructureID id = entry->structureID) {
            Structure* structure = id.decode();
            visitor.appendUnbarriered(structure);
        }
    }
}

} // namespace JSC
