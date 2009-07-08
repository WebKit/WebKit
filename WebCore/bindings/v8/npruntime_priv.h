/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef npruntime_priv_h
#define npruntime_priv_h

#include "third_party/npapi/bindings/npruntime.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
    _NPN_InitializeVariantWithStringCopy() will copy string data.  The string data
    will be deallocated by calls to NPReleaseVariantValue().
*/
void _NPN_InitializeVariantWithStringCopy(NPVariant*, const NPString*);
void _NPN_DeallocateObject(NPObject*);

// The following routines allow the browser to aggressively cleanup NPObjects
// on a per plugin basis.  All NPObjects used through the NPRuntime API should
// be "registered" while they are alive.  After an object has been
// deleted, it is possible for Javascript to have a reference to that object
// which has not yet been garbage collected.  Javascript access to NPObjects
// will reference this registry to determine if the object is accessible or
// not.

// Windows introduces an additional complication for objects created by the
// plugin.  Plugins load inside of a DLL.  Each DLL has it's own heap.  If
// the browser unloads the plugin DLL, all objects created within the DLL's
// heap instantly become invalid.  Normally, when WebKit drops the reference
// on the top-level plugin object, it tells the plugin manager that the
// plugin can be destroyed, which can unload the DLL.  So, we must eliminate
// all pointers to any object ever created by the plugin.

// We generally associate NPObjects with an owner.  The owner of an NPObject
// is an NPObject which, when destroyed, also destroys all objects it owns.
// For example, if an NPAPI plugin creates 10 sub-NPObjects, all 11 objects
// (the NPAPI plugin + its 10 sub-objects) should become inaccessible
// simultaneously.

// The ownership hierarchy is flat, and not a tree.  Imagine the following
// object creation:
//     PluginObject
//          |
//          +-- Creates -----> Object1
//                                |
//                                +-- Creates -----> Object2
//
// PluginObject will be the "owner" for both Object1 and Object2.

// Register an NPObject with the runtime.  If the owner is NULL, the
// object is treated as an owning object.  If owner is not NULL,
// this object will be registered as owned by owner's top-level owner.
void _NPN_RegisterObject(NPObject*, NPObject* owner);

// Unregister an NPObject with the runtime.  If obj is an owning
// object, this call will also unregister all of the owned objects.
void _NPN_UnregisterObject(NPObject*);

// Check to see if an object is registered with the runtime.
// Return true if registered, false otherwise.
bool _NPN_IsAlive(NPObject*);

#ifdef __cplusplus
}  /* end extern "C" */
#endif

#endif // npruntime_priv_h
