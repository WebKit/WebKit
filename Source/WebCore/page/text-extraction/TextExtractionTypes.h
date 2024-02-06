/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#pragma once

#include "FloatRect.h"
#include "FloatSize.h"
#include <wtf/Forward.h>

namespace WebCore {
namespace TextExtraction {

struct TextItemData {
    String content;
};

struct ScrollableItemData {
    FloatSize contentSize;
};

struct EditableItemData {
    bool isFocused { false };
};

struct ImageItemData {
    String name;
    String altText;
};

struct InteractiveItemData {
    bool isEnabled { false };
};

enum class ContainerType : uint8_t {
    Root,
    ViewportConstrained,
    Link,
    List,
    ListItem,
    BlockQuote,
    Article,
    Section,
    Nav,
};

using ItemData = std::variant<ContainerType, TextItemData, ScrollableItemData, EditableItemData, ImageItemData, InteractiveItemData>;

struct Item {
    ItemData data;
    FloatRect rectInRootView;
    Vector<Item> children;
};

} // namespace TextExtraction
} // namespace WebCore
