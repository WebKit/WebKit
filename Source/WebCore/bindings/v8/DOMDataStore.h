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

#ifndef DOMDataStore_h
#define DOMDataStore_h

#include "V8DOMMap.h"

#include <v8.h>
#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Threading.h>
#include <wtf/ThreadSpecific.h>
#include <wtf/Vector.h>

namespace WebCore {

    class DOMData;
    class DOMDataStore;

    typedef WTF::Vector<DOMDataStore*> DOMDataList;

    // DOMDataStore
    //
    // DOMDataStore is the backing store that holds the maps between DOM objects
    // and JavaScript objects.  In general, each thread can have multiple backing
    // stores, one per isolated world.
    //
    // This class doesn't manage the lifetime of the store.  The data store
    // lifetime is managed by subclasses.
    //
    class DOMDataStore {
        WTF_MAKE_NONCOPYABLE(DOMDataStore);
    public:
        enum DOMWrapperMapType {
            DOMNodeMap,
            ActiveDOMNodeMap,
            DOMObjectMap,
            ActiveDOMObjectMap,
        };

        DOMDataStore();
        virtual ~DOMDataStore();

        // A list of all DOMDataStore objects in the current V8 instance (thread). Normally, each World has a DOMDataStore.
        static DOMDataList& allStores();

        void* getDOMWrapperMap(DOMWrapperMapType);

        DOMNodeMapping& domNodeMap() { return *m_domNodeMap; }
        DOMNodeMapping& activeDomNodeMap() { return *m_activeDomNodeMap; }
        DOMWrapperMap<void>& domObjectMap() { return *m_domObjectMap; }
        DOMWrapperMap<void>& activeDomObjectMap() { return *m_activeDomObjectMap; }

        // Need by V8GCController.
        static void weakActiveDOMObjectCallback(v8::Persistent<v8::Value> v8Object, void* domObject);
        static void weakNodeCallback(v8::Persistent<v8::Value> v8Object, void* domObject);

    protected:
        static void weakDOMObjectCallback(v8::Persistent<v8::Value> v8Object, void* domObject);

        DOMNodeMapping* m_domNodeMap;
        DOMNodeMapping* m_activeDomNodeMap;
        DOMWrapperMap<void>* m_domObjectMap;
        DOMWrapperMap<void>* m_activeDomObjectMap;
    };

} // namespace WebCore

#endif // DOMDataStore_h
