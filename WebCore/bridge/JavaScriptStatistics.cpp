/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "JavaScriptStatistics.h"

#include <JavaScriptCore/HashCountedSet.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/collector.h>
#include <JavaScriptCore/interpreter.h>
#include <pthread.h>

namespace WebCore {

using KJS::Collector;
using KJS::Interpreter;
using KJS::JSLock;

static void* collect(void*)
{
    JSLock lock;
    Collector::collect();
    return 0;
}

size_t JavaScriptStatistics::objectCount()
{
    JSLock lock;
    return Collector::size();
}

size_t JavaScriptStatistics::interpreterCount()
{
    JSLock lock;
    return Collector::numInterpreters();
}

size_t JavaScriptStatistics::protectedObjectCount()
{
    JSLock lock;
    return Collector::numProtectedObjects();
}

HashCountedSet<const char*>* JavaScriptStatistics::rootObjectTypeCounts()
{
    JSLock lock;
    
    return Collector::rootObjectTypeCounts();
}

void JavaScriptStatistics::garbageCollect()
{
    collect(0);
}

void JavaScriptStatistics::garbageCollectOnAlternateThread(bool waitUntilDone)
{
    pthread_t thread;
    pthread_create(&thread, NULL, collect, NULL);

    if (waitUntilDone) {
        JSLock::DropAllLocks dropLocks; // Otherwise our lock would deadlock the collect thread we're joining
        pthread_join(thread, NULL);
    }
}

bool JavaScriptStatistics::shouldPrintExceptions()
{
    JSLock lock;
    return Interpreter::shouldPrintExceptions();
}

void JavaScriptStatistics::setShouldPrintExceptions(bool print)
{
    JSLock lock;
    Interpreter::setShouldPrintExceptions(print);
}

} // namespace WebCore
