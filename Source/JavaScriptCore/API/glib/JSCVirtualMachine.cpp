/*
 * Copyright (C) 2018 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "JSCVirtualMachine.h"

#include "JSCContextPrivate.h"
#include "JSCVirtualMachinePrivate.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/glib/WTFGType.h>

/**
 * SECTION: JSCVirtualMachine
 * @short_description: JavaScript Virtual Machine
 * @title: JSCVirtualMachine
 * @see_also: JSCContext
 *
 * JSCVirtualMachine represents a group of JSCContext<!-- -->s. It allows
 * concurrent JavaScript execution by creating a different instance of
 * JSCVirtualMachine in each thread.
 *
 * To create a group of JSCContext<!-- -->s pass the same JSCVirtualMachine
 * instance to every JSCContext constructor.
 */

struct _JSCVirtualMachinePrivate {
    JSContextGroupRef jsContextGroup;
    HashMap<JSGlobalContextRef, JSCContext*> contextCache;
};

WEBKIT_DEFINE_TYPE(JSCVirtualMachine, jsc_virtual_machine, G_TYPE_OBJECT)

static Lock wrapperCacheMutex;

static HashMap<JSContextGroupRef, JSCVirtualMachine*>& wrapperMap()
{
    static NeverDestroyed<HashMap<JSContextGroupRef, JSCVirtualMachine*>> map;
    return map;
}

static void addWrapper(JSContextGroupRef group, JSCVirtualMachine* vm)
{
    auto locker = holdLock(wrapperCacheMutex);
    ASSERT(!wrapperMap().contains(group));
    wrapperMap().set(group, vm);
}

static void removeWrapper(JSContextGroupRef group)
{
    auto locker = holdLock(wrapperCacheMutex);
    ASSERT(wrapperMap().contains(group));
    wrapperMap().remove(group);
}

static void jscVirtualMachineSetContextGroup(JSCVirtualMachine *vm, JSContextGroupRef group)
{
    if (group) {
        ASSERT(!vm->priv->jsContextGroup);
        vm->priv->jsContextGroup = group;
        JSContextGroupRetain(vm->priv->jsContextGroup);
        addWrapper(vm->priv->jsContextGroup, vm);
    } else if (vm->priv->jsContextGroup) {
        removeWrapper(vm->priv->jsContextGroup);
        JSContextGroupRelease(vm->priv->jsContextGroup);
        vm->priv->jsContextGroup = nullptr;
    }
}

static void jscVirtualMachineEnsureContextGroup(JSCVirtualMachine *vm)
{
    if (vm->priv->jsContextGroup)
        return;

    auto* jsContextGroup = JSContextGroupCreate();
    jscVirtualMachineSetContextGroup(vm, jsContextGroup);
    JSContextGroupRelease(jsContextGroup);
}

static void jscVirtualMachineDispose(GObject* object)
{
    JSCVirtualMachine* vm = JSC_VIRTUAL_MACHINE(object);
    jscVirtualMachineSetContextGroup(vm, nullptr);

    G_OBJECT_CLASS(jsc_virtual_machine_parent_class)->dispose(object);
}

static void jsc_virtual_machine_class_init(JSCVirtualMachineClass* klass)
{
    GObjectClass* objClass = G_OBJECT_CLASS(klass);
    objClass->dispose = jscVirtualMachineDispose;
}

GRefPtr<JSCVirtualMachine> jscVirtualMachineGetOrCreate(JSContextGroupRef jsContextGroup)
{
    GRefPtr<JSCVirtualMachine> vm = wrapperMap().get(jsContextGroup);
    if (!vm) {
        vm = adoptGRef(jsc_virtual_machine_new());
        jscVirtualMachineSetContextGroup(vm.get(), jsContextGroup);
    }
    return vm;
}

JSContextGroupRef jscVirtualMachineGetContextGroup(JSCVirtualMachine* vm)
{
    jscVirtualMachineEnsureContextGroup(vm);
    return vm->priv->jsContextGroup;
}

void jscVirtualMachineAddContext(JSCVirtualMachine* vm, JSCContext* context)
{
    ASSERT(vm->priv->jsContextGroup);
    auto jsContext = jscContextGetJSContext(context);
    ASSERT(JSContextGetGroup(jsContext) == vm->priv->jsContextGroup);
    ASSERT(!vm->priv->contextCache.contains(jsContext));
    vm->priv->contextCache.set(jsContext, context);
}

void jscVirtualMachineRemoveContext(JSCVirtualMachine* vm, JSCContext* context)
{
    ASSERT(vm->priv->jsContextGroup);
    auto jsContext = jscContextGetJSContext(context);
    ASSERT(JSContextGetGroup(jsContext) == vm->priv->jsContextGroup);
    ASSERT(vm->priv->contextCache.contains(jsContext));
    vm->priv->contextCache.remove(jsContext);
}

JSCContext* jscVirtualMachineGetContext(JSCVirtualMachine* vm, JSGlobalContextRef jsContext)
{
    return vm->priv->contextCache.get(jsContext);
}

/**
 * jsc_virtual_machine_new:
 *
 * Create a new #JSCVirtualMachine.
 *
 * Returns: (transfer full): the newly created #JSCVirtualMachine.
 */
JSCVirtualMachine* jsc_virtual_machine_new()
{
    return JSC_VIRTUAL_MACHINE(g_object_new(JSC_TYPE_VIRTUAL_MACHINE, nullptr));
}
