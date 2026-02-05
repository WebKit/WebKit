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

#include <JavaScriptCore/RegularExpression.h>
#include <WebCore/CharacterRange.h>
#include <WebCore/FloatRect.h>
#include <WebCore/FloatSize.h>
#include <WebCore/IntPoint.h>
#include <WebCore/NodeIdentifier.h>
#include <WebCore/WebKitJSHandle.h>
#include <wtf/Forward.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/URL.h>
#include <wtf/UniqueRef.h>

#if ENABLE(DATA_DETECTION)
#include <WebCore/DataDetectorType.h>
#endif

namespace WebCore {

struct FrameIdentifierType;
using FrameIdentifier = ObjectIdentifier<FrameIdentifierType>;

namespace TextExtraction {

enum class Action : uint8_t {
    Click,
    SelectText,
    SelectMenuItem,
    TextInput,
    KeyPress,
    HighlightText,
    ScrollBy,
};

struct Interaction {
    Action action { Action::Click };
    String text;
    std::optional<FloatPoint> locationInRootView;
    std::optional<NodeIdentifier> nodeIdentifier;
    FloatSize scrollDelta;
    bool replaceAll { false };
    bool scrollToVisible { false };
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

enum class NodeIdentifierInclusion : uint8_t {
    None,
    EditableOnly,
    Interactive,
    AllContainers,
};

struct Request {
    HashMap<String, HashMap<JSHandleIdentifier, String>> clientNodeAttributes;
    std::optional<FloatRect> collectionRectInRootView;
    std::optional<JSHandleIdentifier> targetNodeHandleIdentifier;
    Vector<JSHandleIdentifier> handleIdentifiersOfNodesToSkip;
    bool mergeParagraphs { false };
    bool skipNearlyTransparentContent { false };
    NodeIdentifierInclusion nodeIdentifierInclusion { NodeIdentifierInclusion::None };
    bool includeEventListeners { false };
    bool includeAccessibilityAttributes { false };
    bool includeTextInAutoFilledControls { false };
#if ENABLE(DATA_DETECTION)
    OptionSet<DataDetectorType> dataDetectorTypes;
#endif
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
    IntPoint scrollPosition;
    bool isRoot { false };
    bool hasOverflowItems { false };
};

struct ImageItemData {
    URL completedSource;
    String shortenedName;
    String altText;
};

struct LinkItemData {
    String target;
    URL completedURL;
    String shortenedURLString;
};

struct IFrameData {
    String origin;
    FrameIdentifier identifier;
};

struct ContentEditableData {
    bool isPlainTextOnly { false };
    bool isFocused { false };
};

struct FormData {
    String autocomplete;
    String name;
};

struct TextFormControlData {
    Editable editable;
    String controlType;
    String autocomplete;
    String pattern;
    String name;
    std::optional<int> minLength;
    std::optional<int> maxLength;
    bool isRequired { false };
    bool isReadonly { false };
    bool isDisabled { false };
    bool isChecked { false };
};

struct SelectOptionData {
    String value;
    String label;
    bool isSelected { false };
};

struct SelectData {
    Vector<SelectOptionData> options;
    bool isMultiple { false };
};

enum class ContainerType : uint8_t {
    ViewportConstrained,
    List,
    ListItem,
    BlockQuote,
    Article,
    Section,
    Nav,
    Button,
    Canvas,
    Subscript,
    Superscript,
    Strikethrough,
    Generic,
};

using ItemData = Variant<ContainerType, TextItemData, ScrollableItemData, ImageItemData, SelectData, ContentEditableData, TextFormControlData, FormData, LinkItemData, IFrameData>;

struct Item {
    ItemData data;
    FloatRect rectInRootView;
    Vector<Item> children;
    String nodeName;
    std::optional<NodeIdentifier> nodeIdentifier;
    std::optional<FrameIdentifier> frameIdentifier;
    OptionSet<EventListenerCategory> eventListeners;
    HashMap<String, String> ariaAttributes;
    String accessibilityRole;
    String title;
    HashMap<String, String> clientAttributes;
    unsigned enclosingBlockNumber { 0 };

    template<typename T> bool hasData() const
    {
        return std::holds_alternative<T>(data);
    }

    template<typename T> std::optional<T> dataAs() const
    {
        if (hasData<T>())
            return std::get<T>(data);
        return std::nullopt;
    }
};

struct Result {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED_EXPORT(Result, WEBCORE_EXPORT);

    Item rootItem;
    unsigned visibleTextLength { 0 };
};

struct PageResults {
    Result mainFrameResult;
    HashMap<FrameIdentifier, UniqueRef<Result>> subFrameResults;
};

WEBCORE_EXPORT Result collatePageResults(PageResults&&);

struct FilterRuleData {
    String name;
    String urlPatternString;
    String scriptSource;
};

enum class FilterRulePattern : uint8_t { Global };

struct FilterRule {
    String name;
    Variant<FilterRulePattern, JSC::Yarr::RegularExpression> urlPattern;
    String scriptSource;
};

} // namespace TextExtraction
} // namespace WebCore
