#include "config.h"
#include "ShadowRealm.h"

#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ShadowRealm);

ShadowRealm::ShadowRealm(String&&, std::optional<std::variant<String, WorkerOptions>>&&)
{
}

MessagePort* ShadowRealm::port() const
{
    // FIXME: implement.
    return nullptr;
}

ScriptExecutionContext* ShadowRealm::scriptExecutionContext() const
{
    // FIXME: implement.
    return nullptr;
}

EventTargetInterface ShadowRealm::eventTargetInterface() const
{
    return SharedWorkerEventTargetInterfaceType;
}

} // namespace WebCore
