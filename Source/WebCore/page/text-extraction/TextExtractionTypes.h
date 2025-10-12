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

#include <WebCore/CharacterRange.h>
#include <WebCore/FloatRect.h>
#include <WebCore/FloatSize.h>
#include <WebCore/NodeIdentifier.h>
#include <wtf/Forward.h>
#include <wtf/URL.h>

namespace WebCore {
namespace TextExtraction {

enum class Action : uint8_t {
    Click,
    SelectText,
    SelectMenuItem,
    TextInput,
    KeyPress,
};

struct Interaction {
    Action action { Action::Click };
    String text;
    std::optional<FloatPoint> locationInRootView;
    std::optional<NodeIdentifier> nodeIdentifier;
    bool replaceAll { false };
};

struct ExtractedText {
    String text;
    std::optional<NodeIdentifier> nodeIdentifier;
};

struct InteractionDescription {
    String description;
    Vector<String> stringsToValidate;
};

enum class EventListenerCategory : uint8_t {
    Click       = 1 << 0,
    Hover       = 1 << 1,
    Touch       = 1 << 2,
    Wheel       = 1 << 3,
    Keyboard    = 1 << 4,
};

struct Request {
    std::optional<WebCore::FloatRect> collectionRectInRootView;
    bool mergeParagraphs { false };
    bool skipNearlyTransparentContent { false };
    bool canIncludeIdentifiers { false };
};

struct Editable {
    String label;
    String placeholder;
    bool isSecure { false };
    bool isFocused { false };
};

struct TextItemData {
    Vector<std::pair<URL, CharacterRange>> links;
    std::optional<CharacterRange> selectedRange;
    String content;
    std::optional<Editable> editable;
};

struct ScrollableItemData {
    FloatSize contentSize;
};

struct ImageItemData {
    String name;
    String altText;
};

struct LinkItemData {
    String target;
    URL completedURL;
};

struct ContentEditableData {
    bool isPlainTextOnly { false };
    bool isFocused { false };
};

struct TextFormControlData {
    Editable editable;
    String controlType;
    String autocomplete;
    bool isReadonly { false };
    bool isDisabled { false };
    bool isChecked { false };
};

struct SelectData {
    Vector<String> selectedValues;
    bool isMultiple { false };
};

enum class ContainerType : uint8_t {
    Root,
    ViewportConstrained,
    List,
    ListItem,
    BlockQuote,
    Article,
    Section,
    Nav,
    Button,
    Canvas,
    Generic,
};

using ItemData = Variant<ContainerType, TextItemData, ScrollableItemData, ImageItemData, SelectData, ContentEditableData, TextFormControlData, LinkItemData>;

struct Item {
    ItemData data;
    FloatRect rectInRootView;
    Vector<Item> children;
    std::optional<NodeIdentifier> nodeIdentifier;
    OptionSet<EventListenerCategory> eventListeners;
    HashMap<String, String> ariaAttributes;
    String accessibilityRole;
};

} // namespace TextExtraction
} // namespace WebCore
