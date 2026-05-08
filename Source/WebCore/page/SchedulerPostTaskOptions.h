/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 */

#pragma once

#include "AbortSignal.h"
#include "TaskPriority.h"
#include <optional>
#include <wtf/RefPtr.h>

namespace WebCore {

struct SchedulerPostTaskOptions {
    RefPtr<AbortSignal> signal;
    std::optional<TaskPriority> priority;
    uint64_t delay { 0 };
};

} // namespace WebCore
