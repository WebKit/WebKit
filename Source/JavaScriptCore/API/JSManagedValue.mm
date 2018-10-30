/*
 * Copyright (C) 2013, 2016 Apple Inc. All rights reserved.
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


#import "config.h"
#import "JSManagedValue.h"

#if JSC_OBJC_API_ENABLED

#import "APICast.h"
#import "Heap.h"
#import "JSContextInternal.h"
#import "JSValueInternal.h"
#import "JSWeakValue.h"
#import "WeakHandleOwner.h"
#import "ObjcRuntimeExtras.h"
#import "JSCInlines.h"
#import <wtf/NeverDestroyed.h>

class JSManagedValueHandleOwner : public JSC::WeakHandleOwner {
public:
    void finalize(JSC::Handle<JSC::Unknown>, void* context) override;
    bool isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown>, void* context, JSC::SlotVisitor&, const char**) override;
};

static JSManagedValueHandleOwner& managedValueHandleOwner()
{
    static NeverDestroyed<JSManagedValueHandleOwner> jsManagedValueHandleOwner;
    return jsManagedValueHandleOwner;
}

@implementation JSManagedValue {
    JSC::Weak<JSC::JSGlobalObject> m_globalObject;
    RefPtr<JSC::JSLock> m_lock;
    JSC::JSWeakValue m_weakValue;
    NSMapTable *m_owners;
}

+ (JSManagedValue *)managedValueWithValue:(JSValue *)value
{
    return [[[self alloc] initWithValue:value] autorelease];
}

+ (JSManagedValue *)managedValueWithValue:(JSValue *)value andOwner:(id)owner
{
    JSManagedValue *managedValue = [[self alloc] initWithValue:value];
    [value.context.virtualMachine addManagedReference:managedValue withOwner:owner];
    return [managedValue autorelease];
}

- (instancetype)init
{
    return [self initWithValue:nil];
}

- (instancetype)initWithValue:(JSValue *)value
{
    self = [super init];
    if (!self)
        return nil;
    
    if (!value)
        return self;

    JSC::ExecState* exec = toJS([value.context JSGlobalContextRef]);
    JSC::JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    auto& owner = managedValueHandleOwner();
    JSC::Weak<JSC::JSGlobalObject> weak(globalObject, &owner, (__bridge void*)self);
    m_globalObject.swap(weak);

    m_lock = &exec->vm().apiLock();

    NSPointerFunctionsOptions weakIDOptions = NSPointerFunctionsWeakMemory | NSPointerFunctionsObjectPersonality;
    NSPointerFunctionsOptions integerOptions = NSPointerFunctionsOpaqueMemory | NSPointerFunctionsIntegerPersonality;
    m_owners = [[NSMapTable alloc] initWithKeyOptions:weakIDOptions valueOptions:integerOptions capacity:1];

    JSC::JSValue jsValue = toJS(exec, [value JSValueRef]);
    if (jsValue.isObject())
        m_weakValue.setObject(JSC::jsCast<JSC::JSObject*>(jsValue.asCell()), owner, (__bridge void*)self);
    else if (jsValue.isString())
        m_weakValue.setString(JSC::jsCast<JSC::JSString*>(jsValue.asCell()), owner, (__bridge void*)self);
    else
        m_weakValue.setPrimitive(jsValue);
    return self;
}

- (void)dealloc
{
    JSVirtualMachine *virtualMachine = [[[self value] context] virtualMachine];
    if (virtualMachine) {
        NSMapTable *copy = [m_owners copy];
        for (id owner in [copy keyEnumerator]) {
            size_t count = reinterpret_cast<size_t>(NSMapGet(m_owners, (__bridge void*)owner));
            while (count--)
                [virtualMachine removeManagedReference:self withOwner:owner];
        }
        [copy release];
    }

    [self disconnectValue];
    [m_owners release];
    [super dealloc];
}

- (void)didAddOwner:(id)owner
{
    size_t count = reinterpret_cast<size_t>(NSMapGet(m_owners, (__bridge void*)owner));
    NSMapInsert(m_owners, (__bridge void*)owner, reinterpret_cast<void*>(count + 1));
}

- (void)didRemoveOwner:(id)owner
{
    size_t count = reinterpret_cast<size_t>(NSMapGet(m_owners, (__bridge void*)owner));

    if (!count)
        return;

    if (count == 1) {
        NSMapRemove(m_owners, (__bridge void*)owner);
        return;
    }

    NSMapInsert(m_owners, (__bridge void*)owner, reinterpret_cast<void*>(count - 1));
}

- (JSValue *)value
{
    WTF::Locker<JSC::JSLock> locker(m_lock.get());
    JSC::VM* vm = m_lock->vm();
    if (!vm)
        return nil;

    JSC::JSLockHolder apiLocker(vm);
    if (!m_globalObject)
        return nil;
    if (m_weakValue.isClear())
        return nil;
    JSC::ExecState* exec = m_globalObject->globalExec();
    JSContext *context = [JSContext contextWithJSGlobalContextRef:toGlobalRef(exec)];
    JSC::JSValue value;
    if (m_weakValue.isPrimitive())
        value = m_weakValue.primitive();
    else if (m_weakValue.isString())
        value = m_weakValue.string();
    else
        value = m_weakValue.object();
    return [JSValue valueWithJSValueRef:toRef(exec, value) inContext:context];
}

- (void)disconnectValue
{
    m_globalObject.clear();
    m_weakValue.clear();
}

@end

@interface JSManagedValue (PrivateMethods)
- (void)disconnectValue;
@end

bool JSManagedValueHandleOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown>, void* context, JSC::SlotVisitor& visitor, const char** reason)
{
    if (UNLIKELY(reason))
        *reason = "JSManagedValue is opaque root";
    JSManagedValue *managedValue = (__bridge JSManagedValue *)context;
    return visitor.containsOpaqueRoot((__bridge void*)managedValue);
}

void JSManagedValueHandleOwner::finalize(JSC::Handle<JSC::Unknown>, void* context)
{
    JSManagedValue *managedValue = (__bridge JSManagedValue *)context;
    [managedValue disconnectValue];
}

#endif // JSC_OBJC_API_ENABLED
