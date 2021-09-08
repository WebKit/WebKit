#pragma once

#include "InternalFunction.h"
#include "ShadowRealmPrototype.h"

namespace JSC {

class GetterSetter;

class ShadowRealmConstructor final : public InternalFunction {
public:
    typedef InternalFunction Base;
    static constexpr unsigned StructureFlags = Base::StructureFlags | HasStaticPropertyTable;

    static ShadowRealmConstructor* create(VM& vm, Structure* structure, ShadowRealmPrototype* shadowRealmPrototype, GetterSetter*)
    {
        ShadowRealmConstructor* constructor = new (NotNull, allocateCell<ShadowRealmConstructor>(vm.heap)) ShadowRealmConstructor(vm, structure);
        constructor->finishCreation(vm, shadowRealmPrototype);
        return constructor;
    }

    DECLARE_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
    }

private:
    ShadowRealmConstructor(VM&, Structure*);
    void finishCreation(VM&, ShadowRealmPrototype*);
};
STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(ShadowRealmConstructor, InternalFunction);

} // namespace JSC

