#include "config.h"
#include "ShadowRealmConstructor.h"

#include "ShadowRealmObject.h"
#include "JSCInlines.h"


namespace JSC {

const ClassInfo ShadowRealmConstructor::s_info = { "Function", &InternalFunction::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ShadowRealmConstructor) };

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(ShadowRealmConstructor);

static JSC_DECLARE_HOST_FUNCTION(callShadowRealm);
static JSC_DECLARE_HOST_FUNCTION(constructWithShadowRealmConstructor);

ShadowRealmConstructor::ShadowRealmConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, callShadowRealm, constructWithShadowRealmConstructor)
{
}

void ShadowRealmConstructor::finishCreation(VM& vm, ShadowRealmPrototype* shadowRealmPrototype)
{
    Base::finishCreation(vm, 0, vm.propertyNames->ShadowRealm.string(), PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, shadowRealmPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
}

JSObject* constructShadowRealm(JSGlobalObject* globalObject, JSValue, const ArgList&)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    Structure* shadowRealmStructure = ShadowRealmObject::createStructure(vm, globalObject, globalObject->shadowRealmPrototype());
    return ShadowRealmObject::create(vm, shadowRealmStructure, globalObject->globalObjectMethodTable());
}

JSC_DEFINE_HOST_FUNCTION(constructWithShadowRealmConstructor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ArgList args(callFrame);
    return JSValue::encode(constructShadowRealm(globalObject, callFrame->newTarget(), args));
}

JSC_DEFINE_HOST_FUNCTION(callShadowRealm, (JSGlobalObject* globalObject, CallFrame*))
{
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "ShadowRealm"));
}

} // namespace JSC

