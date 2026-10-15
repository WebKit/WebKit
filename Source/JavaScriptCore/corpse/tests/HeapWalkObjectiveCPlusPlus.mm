/*
 * Copyright (C) 2026 Igalia S.L.
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
#import "HeapWalkObjectiveC.h"

#if OS(DARWIN) && ENABLE(MYA_HEAP)

#import <JavaScriptCore/CorpseAddress.h>
#import <wtf/Function.h>

@interface HeapWalkObjectiveCPlusPlusObject : NSObject {
@public
    id strongObject;
    __weak id weakObject;
    std::unique_ptr<JSCToolsTest::ObjectiveCHolderLeaf> ownedLeaf;
}
- (Function<void()>)callbackHolding:(JSCToolsTest::ObjectiveCHolderLeaf*)leaf;
@end

@implementation HeapWalkObjectiveCPlusPlusObject
- (Function<void()>)callbackHolding:(JSCToolsTest::ObjectiveCHolderLeaf*)leaf
{
    return [leaf] {
        ++leaf->value;
    };
}
@end

namespace JSCToolsTest {

ObjectiveCHolderInterface::~ObjectiveCHolderInterface() = default;

namespace {

class ObjectiveCHolder final : public ObjectiveCHolderInterface {
public:
    ObjectiveCHolder()
    {
        object->strongObject = plainObject;
        object->weakObject = plainObject;
        object->ownedLeaf = makeUniqueWithoutFastMallocCheck<ObjectiveCHolderLeaf>();
    }

    HeapWalkObjectiveCPlusPlusObject* object { [HeapWalkObjectiveCPlusPlusObject new] };
    HeapWalkPlainObject* plainObject { [HeapWalkPlainObject new] };
    void (^block)() { ^{ } };
    Function<void()> callback;
};

} // anonymous namespace

ObjectiveCHolderInterface* createObjectiveCHolder(ObjectiveCHolderAddresses& addresses)
{
    auto* holder = new ObjectiveCHolder;
    auto* leaf = new ObjectiveCHolderLeaf;
    holder->callback = [holder->object callbackHolding:leaf];
    addresses = {
        JSC::Corpse::Address { holder }.toTargetVMAddress(),
        JSC::Corpse::Address { (__bridge void*)holder->object }.toTargetVMAddress(),
        JSC::Corpse::Address { (__bridge void*)holder->plainObject }.toTargetVMAddress(),
        JSC::Corpse::Address { (__bridge void*)holder->block }.toTargetVMAddress(),
        JSC::Corpse::Address { leaf }.toTargetVMAddress(),
    };
    return holder;
}

} // namespace JSCToolsTest

#endif // OS(DARWIN) && ENABLE(MYA_HEAP)
