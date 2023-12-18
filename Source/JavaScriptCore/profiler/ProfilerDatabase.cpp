/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ProfilerDatabase.h"

#include "CodeBlock.h"
#include "JSCInlines.h"
#include "JSONObject.h"
#include "ObjectConstructor.h"
#include "ProfilerDumper.h"
#include <wtf/FilePrintStream.h>
#include <wtf/TZoneMallocInlines.h>

namespace JSC { namespace Profiler {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Database);

static std::atomic<int> databaseCounter;

static Lock registrationLock;
static std::atomic<int> didRegisterAtExit;
static Database* firstDatabase;

Database::Database(VM& vm)
    : m_databaseID(++databaseCounter)
    , m_vm(vm)
    , m_shouldSaveAtExit(false)
    , m_nextRegisteredDatabase(nullptr)
{
}

Database::~Database()
{
    if (m_shouldSaveAtExit) {
        removeDatabaseFromAtExit();
        performAtExitSave();
    }
}

Bytecodes* Database::ensureBytecodesFor(CodeBlock* codeBlock)
{
    Locker locker { m_lock };
    return ensureBytecodesFor(locker, codeBlock);
}

Bytecodes* Database::ensureBytecodesFor(const AbstractLocker&, CodeBlock* codeBlock)
{
    codeBlock = codeBlock->baselineAlternative();
    
    HashMap<CodeBlock*, Bytecodes*>::iterator iter = m_bytecodesMap.find(codeBlock);
    if (iter != m_bytecodesMap.end())
        return iter->value;
    
    m_bytecodes.append(Bytecodes(m_bytecodes.size(), codeBlock));
    Bytecodes* result = &m_bytecodes.last();
    
    m_bytecodesMap.add(codeBlock, result);
    
    return result;
}

void Database::notifyDestruction(CodeBlock* codeBlock)
{
    Locker locker { m_lock };
    
    m_bytecodesMap.remove(codeBlock);
    m_compilationMap.remove(codeBlock);
}

void Database::addCompilation(CodeBlock* codeBlock, Ref<Compilation>&& compilation)
{
    Locker locker { m_lock };

    m_compilations.append(compilation.copyRef());
    m_compilationMap.set(codeBlock, WTFMove(compilation));
}

Ref<JSON::Value> Database::toJSON() const
{
    Dumper dumper(*this);
    auto result = JSON::Object::create();

    auto bytecodes = JSON::Array::create();
    for (unsigned i = 0; i < m_bytecodes.size(); ++i)
        bytecodes->pushValue(m_bytecodes[i].toJSON(dumper));
    result->setValue(dumper.keys().m_bytecodes, WTFMove(bytecodes));

    auto compilations = JSON::Array::create();
    for (unsigned i = 0; i < m_compilations.size(); ++i)
        compilations->pushValue(m_compilations[i]->toJSON(dumper));
    result->setValue(dumper.keys().m_compilations, WTFMove(compilations));

    auto events = JSON::Array::create();
    for (unsigned i = 0; i < m_events.size(); ++i)
        events->pushValue(m_events[i].toJSON(dumper));
    result->setValue(dumper.keys().m_events, WTFMove(events));

    return result;
}

bool Database::save(const char* filename) const
{
    auto out = FilePrintStream::open(filename, "w");
    if (!out)
        return false;
    out->print(toJSON().get());
    return true;
}

void Database::registerToSaveAtExit(const char* filename)
{
    m_atExitSaveFilename = filename;
    
    if (m_shouldSaveAtExit)
        return;
    
    addDatabaseToAtExit();
    m_shouldSaveAtExit = true;
}

void Database::logEvent(CodeBlock* codeBlock, const char* summary, const CString& detail)
{
    Locker locker { m_lock };
    
    Bytecodes* bytecodes = ensureBytecodesFor(locker, codeBlock);
    Compilation* compilation = m_compilationMap.get(codeBlock);
    m_events.append(Event(WallTime::now(), bytecodes, compilation, summary, detail));
}

void Database::addDatabaseToAtExit()
{
    if (++didRegisterAtExit == 1)
        atexit(atExitCallback);
    
    Locker locker { registrationLock };
    m_nextRegisteredDatabase = firstDatabase;
    firstDatabase = this;
}

void Database::removeDatabaseFromAtExit()
{
    Locker locker { registrationLock };
    for (Database** current = &firstDatabase; *current; current = &(*current)->m_nextRegisteredDatabase) {
        if (*current != this)
            continue;
        *current = m_nextRegisteredDatabase;
        m_nextRegisteredDatabase = nullptr;
        m_shouldSaveAtExit = false;
        break;
    }
}

void Database::performAtExitSave() const
{
    JSLockHolder lock(m_vm);
    save(m_atExitSaveFilename.data());
}

Database* Database::removeFirstAtExitDatabase()
{
    Locker locker { registrationLock };
    Database* result = firstDatabase;
    if (result) {
        firstDatabase = result->m_nextRegisteredDatabase;
        result->m_nextRegisteredDatabase = nullptr;
        result->m_shouldSaveAtExit = false;
    }
    return result;
}

void Database::atExitCallback()
{
    while (Database* database = removeFirstAtExitDatabase())
        database->performAtExitSave();
}

} } // namespace JSC::Profiler

