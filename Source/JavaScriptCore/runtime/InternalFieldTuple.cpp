#include "config.h"
#include "InternalFieldTuple.h"
#include "JSCInlines.h"
#include "JSGlobalObject.h"
#include "JSInternalFieldObjectImplInlines.h"
#include "StructureInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(InternalFieldTuple);

const ClassInfo InternalFieldTuple::s_info = { "InternalFieldTuple"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(InternalFieldTuple) };

InternalFieldTuple::InternalFieldTuple(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

Structure* InternalFieldTuple::createStructure(VM& vm, JSGlobalObject* globalObject)
{
    return Structure::create(vm, globalObject, jsNull(), TypeInfo(InternalFieldTupleType, Base::StructureFlags), info());
}

template<typename Visitor>
void InternalFieldTuple::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<InternalFieldTuple*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
}

DEFINE_VISIT_CHILDREN_WITH_MODIFIER(JS_EXPORT_PRIVATE, InternalFieldTuple);

} // namespace JSC
