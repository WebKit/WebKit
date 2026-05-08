/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 */

#pragma once

namespace WebCore {

enum class TaskPriority : uint8_t {
    UserBlocking,
    UserVisible,
    Background,
};

} // namespace WebCore
