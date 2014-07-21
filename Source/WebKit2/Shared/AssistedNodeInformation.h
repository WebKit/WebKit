/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef AssistedNodeInformation_h
#define AssistedNodeInformation_h

#include "ArgumentCoders.h"
#include <WebCore/IntRect.h>
#include <WebCore/WebAutocapitalize.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

enum class InputType {
    None,
    ContentEditable,
    Text,
    Password,
    TextArea,
    Search,
    Email,
    URL,
    Phone,
    Number,
    NumberPad,
    Date,
    DateTime,
    DateTimeLocal,
    Month,
    Week,
    Time,
    Select
};

#if PLATFORM(IOS)
struct OptionItem {
    OptionItem()
        : isGroup(false)
        , isSelected(false)
        , disabled(false)
        , parentGroupID(0)
    {
    }

    OptionItem(const OptionItem& item)
        : text(item.text)
        , isGroup(item.isGroup)
        , isSelected(item.isSelected)
        , disabled(item.disabled)
        , parentGroupID(item.parentGroupID)
    {
    }

    OptionItem(const String& text, bool isGroup, int parentID, bool selected, bool disabled)
        : text(text)
        , isGroup(isGroup)
        , isSelected(selected)
        , disabled(disabled)
        , parentGroupID(parentID)
    {
    }
    String text;
    bool isGroup;
    bool isSelected;
    bool disabled;
    int parentGroupID;

    void encode(IPC::ArgumentEncoder&) const;
    static bool decode(IPC::ArgumentDecoder&, OptionItem&);
};

struct AssistedNodeInformation {
    AssistedNodeInformation()
        : minimumScaleFactor(-INFINITY)
        , maximumScaleFactor(INFINITY)
        , nodeFontSize(0)
        , hasNextNode(false)
        , hasPreviousNode(false)
        , isAutocorrect(false)
        , isMultiSelect(false)
        , isReadOnly(false)
        , allowsUserScaling(false)
        , insideFixedPosition(false)
        , autocapitalizeType(WebAutocapitalizeTypeDefault)
        , elementType(InputType::None)
        , selectedIndex(-1)
        , valueAsNumber(0)
    {
    }

    WebCore::IntRect elementRect;
    WebCore::IntRect selectionRect;
    double minimumScaleFactor;
    double maximumScaleFactor;
    double nodeFontSize;
    bool hasNextNode;
    bool hasPreviousNode;
    bool isAutocorrect;
    bool isMultiSelect;
    bool isReadOnly;
    bool allowsUserScaling;
    bool insideFixedPosition;
    WebAutocapitalizeType autocapitalizeType;
    InputType elementType;
    String formAction;
    Vector<OptionItem> selectOptions;
    int selectedIndex;
    String value;
    double valueAsNumber;
    String title;

    void encode(IPC::ArgumentEncoder&) const;
    static bool decode(IPC::ArgumentDecoder&, AssistedNodeInformation&);
};
#endif

}

#endif // InteractionInformationAtPosition_h
