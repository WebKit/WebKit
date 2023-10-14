#pragma once

#include "JSInternalFieldObjectImpl.h"

namespace JSC {

// InternalFieldTuple is a generic InternalFieldObject that currently has two internal fields
// It is used in
// - On the global object to store a read/write ref to Bun's AsyncLocalStorage context
// - Within PromiseOperations.js for AsyncLocalStorage related stuff
class InternalFieldTuple : public JSInternalFieldObjectImpl<2> {
protected:
    JS_EXPORT_PRIVATE InternalFieldTuple(VM&, Structure*);
    DECLARE_DEFAULT_FINISH_CREATION;
    DECLARE_VISIT_CHILDREN_WITH_MODIFIER(JS_EXPORT_PRIVATE);

public:
    using Base = JSInternalFieldObjectImpl<numberOfInternalFields>;

    enum class Field : uint8_t {
        Slot0 = 0,
        Slot1,
    };
    const WriteBarrier<Unknown>& internalField(Field field) const { return Base::internalField(static_cast<uint32_t>(field)); }
    WriteBarrier<Unknown>& internalField(Field field) { return Base::internalField(static_cast<uint32_t>(field)); }

    static size_t allocationSize(Checked<size_t> inlineCapacity)
    {
        ASSERT_UNUSED(inlineCapacity, inlineCapacity == 0U);
        return sizeof(InternalFieldTuple);
    }

    template<typename, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return &vm.internalFieldTupleSpace();
    }

    static std::array<JSValue, numberOfInternalFields> initialValues()
    {
        return { {
            jsUndefined(),
            jsUndefined(),
        } };
    }

    static InternalFieldTuple* create(VM& vm, Structure* structure)
    {
        InternalFieldTuple* fields = new (NotNull, allocateCell<InternalFieldTuple>(vm)) InternalFieldTuple(vm, structure);
        fields->finishCreation(vm);
        fields->putInternalField(vm, 0, jsUndefined());
        fields->putInternalField(vm, 1, jsUndefined());
        return fields;
    }

    static inline InternalFieldTuple* create(VM& vm, Structure* structure, JSValue a, JSValue b)
    {
        InternalFieldTuple* fields = create(vm, structure);
        fields->putInternalField(vm, 0, a);
        fields->putInternalField(vm, 1, b);
        return fields;
    }

    static Structure* createStructure(VM&, JSGlobalObject*);

    // These two are equivilent to @getInteralField and @putInternalField
    inline JSValue getInternalField(unsigned index) const
    {
        ASSERT(index < numberOfInternalFields);
        return m_internalFields[index].get();
    }

    inline void putInternalField(VM& vm, unsigned index, JSValue value)
    {
        ASSERT(index < numberOfInternalFields);
        m_internalFields[index].set(vm, this, value);
    }

    DECLARE_EXPORT_INFO;
};

} // namespace JSC
