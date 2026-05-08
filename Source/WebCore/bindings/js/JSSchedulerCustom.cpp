/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 */

#include "config.h"
#include "JSScheduler.h"

#include <JavaScriptCore/SlotVisitorMacros.h>

namespace WebCore {

template<typename Visitor>
void JSScheduler::visitAdditionalChildrenInGCThread(Visitor& visitor)
{
    wrapped().visitAdditionalChildren(visitor);
}

DEFINE_VISIT_ADDITIONAL_CHILDREN_IN_GC_THREAD(JSScheduler);

} // namespace WebCore
