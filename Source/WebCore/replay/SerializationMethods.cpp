/*
 * Copyright (C) 2012 University of Washington. All rights reserved.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SerializationMethods.h"

#if ENABLE(WEB_REPLAY)

#include "AllReplayInputs.h"
#include "Document.h"
#include "Frame.h"
#include "FrameTree.h"
#include "MainFrame.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "PluginData.h"
#include "ReplayInputTypes.h"
#include "SecurityOrigin.h"
#include "URL.h"
#include <wtf/text/Base64.h>

using WebCore::KeypressCommand;
using WebCore::IntPoint;
using WebCore::MimeClassInfo;
using WebCore::MouseButton;
using WebCore::PlatformEvent;
using WebCore::PlatformKeyboardEvent;
using WebCore::PlatformMouseEvent;
using WebCore::PlatformWheelEvent;
using WebCore::PlatformWheelEventGranularity;
using WebCore::PluginData;
using WebCore::PluginInfo;
using WebCore::SecurityOrigin;
using WebCore::URL;
using WebCore::inputTypes;

#if PLATFORM(COCOA)
using WebCore::PlatformWheelEventPhase;
#endif

#define IMPORT_FROM_WEBCORE_NAMESPACE(name) \
using WebCore::name; \

WEB_REPLAY_INPUT_NAMES_FOR_EACH(IMPORT_FROM_WEBCORE_NAMESPACE)
#undef IMPORT_FROM_WEBCORE_NAMESPACE

namespace WebCore {

unsigned long frameIndexFromDocument(const Document* document)
{
    ASSERT(document);
    ASSERT(document->frame());
    return frameIndexFromFrame(document->frame());
}

unsigned long frameIndexFromFrame(const Frame* targetFrame)
{
    ASSERT(targetFrame);

    unsigned long currentIndex = 0;
    const Frame* mainFrame = &targetFrame->tree().top();
    for (const Frame* frame = mainFrame; frame; ++currentIndex, frame = frame->tree().traverseNext(mainFrame)) {
        if (frame == targetFrame)
            return currentIndex;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

Document* documentFromFrameIndex(Page* page, unsigned long frameIndex)
{
    Frame* frame = frameFromFrameIndex(page, frameIndex);
    return frame ? frame->document() : nullptr;
}

Frame* frameFromFrameIndex(Page* page, unsigned long frameIndex)
{
    ASSERT(page);
    ASSERT(frameIndex >= 0);

    MainFrame* mainFrame = &page->mainFrame();
    Frame* frame = mainFrame;
    unsigned long currentIndex = 0;
    for (; currentIndex < frameIndex && frame; ++currentIndex, frame = frame->tree().traverseNext(mainFrame)) { }

    return frame;
}

} // namespace WebCore

#define ENCODE_TYPE_WITH_KEY(_encodedValue, _type, _key, _value) \
    _encodedValue.put<_type>(ASCIILiteral(#_key), _value)

#define ENCODE_OPTIONAL_TYPE_WITH_KEY(_encodedValue, _type, _key, _value, condition) \
    if (condition) \
        ENCODE_TYPE_WITH_KEY(_encodedValue, _type, _key, _value)

#define DECODE_TYPE_WITH_KEY_TO_LVALUE(_encodedValue, _type, _key, _lvalue) \
    if (!_encodedValue.get<_type>(ASCIILiteral(#_key), _lvalue)) \
        return false

#define DECODE_TYPE_WITH_KEY(_encodedValue, _type, _key) \
    EncodingTraits<_type>::DecodedType _key; \
    DECODE_TYPE_WITH_KEY_TO_LVALUE(_encodedValue, _type, _key, _key)

#define DECODE_OPTIONAL_TYPE_WITH_KEY_TO_LVALUE(_encodedValue, _type, _key, _lvalue) \
    bool _key ## WasDecoded = _encodedValue.get<_type>(ASCIILiteral(#_key), _lvalue)

#define DECODE_OPTIONAL_TYPE_WITH_KEY(_encodedValue, _type, _key) \
    EncodingTraits<_type>::DecodedType _key; \
    DECODE_OPTIONAL_TYPE_WITH_KEY_TO_LVALUE(_encodedValue, _type, _key, _key)

namespace JSC {

template<>
EncodedValue EncodingTraits<MimeClassInfo>::encodeValue(const MimeClassInfo& input)
{
    EncodedValue encodedData = EncodedValue::createObject();

    ENCODE_TYPE_WITH_KEY(encodedData, String, type, input.type);
    ENCODE_TYPE_WITH_KEY(encodedData, String, desc, input.desc);
    ENCODE_TYPE_WITH_KEY(encodedData, Vector<String>, extensions, input.extensions);

    return encodedData;
}

template<>
bool EncodingTraits<MimeClassInfo>::decodeValue(EncodedValue& encodedData, MimeClassInfo& input)
{
    MimeClassInfo info;

    DECODE_TYPE_WITH_KEY_TO_LVALUE(encodedData, String, type, info.type);
    DECODE_TYPE_WITH_KEY_TO_LVALUE(encodedData, String, desc, info.desc);
    DECODE_TYPE_WITH_KEY_TO_LVALUE(encodedData, Vector<String>, extensions, info.extensions);

    input = info;
    return true;
}

EncodedValue EncodingTraits<NondeterministicInputBase>::encodeValue(const NondeterministicInputBase& input)
{
    EncodedValue encodedValue = EncodedValue::createObject();
    const AtomicString& type = input.type();

    ENCODE_TYPE_WITH_KEY(encodedValue, String, type, type.string());

#define ENCODE_IF_TYPE_TAG_MATCHES(name) \
    if (type == inputTypes().name) { \
        InputTraits<name>::encode(encodedValue, static_cast<const name&>(input)); \
        return encodedValue; \
    } \

    JS_REPLAY_INPUT_NAMES_FOR_EACH(ENCODE_IF_TYPE_TAG_MATCHES)
    WEB_REPLAY_INPUT_NAMES_FOR_EACH(ENCODE_IF_TYPE_TAG_MATCHES)
#undef ENCODE_IF_TYPE_TAG_MATCHES

    // The macro won't work here because of the class template argument.
    if (type == inputTypes().MemoizedDOMResult) {
        InputTraits<MemoizedDOMResultBase>::encode(encodedValue, static_cast<const MemoizedDOMResultBase&>(input));
        return encodedValue;
    }

    ASSERT_NOT_REACHED();
    return EncodedValue();
}

bool EncodingTraits<NondeterministicInputBase>::decodeValue(EncodedValue& encodedValue, std::unique_ptr<NondeterministicInputBase>& input)
{
    DECODE_TYPE_WITH_KEY(encodedValue, String, type);

#define DECODE_IF_TYPE_TAG_MATCHES(name) \
    if (type == inputTypes().name) { \
        std::unique_ptr<name> decodedInput; \
        if (!InputTraits<name>::decode(encodedValue, decodedInput)) \
            return false; \
        \
        input = std::move(decodedInput); \
        return true; \
    } \

    JS_REPLAY_INPUT_NAMES_FOR_EACH(DECODE_IF_TYPE_TAG_MATCHES)
    WEB_REPLAY_INPUT_NAMES_FOR_EACH(DECODE_IF_TYPE_TAG_MATCHES)
#undef DECODE_IF_TYPE_TAG_MATCHES

    if (type == inputTypes().MemoizedDOMResult) {
        std::unique_ptr<MemoizedDOMResultBase> decodedInput;
        if (!InputTraits<MemoizedDOMResultBase>::decode(encodedValue, decodedInput))
            return false;

        input = std::move(decodedInput);
        return true;
    }

    return false;
}

#if USE(APPKIT)
EncodedValue EncodingTraits<KeypressCommand>::encodeValue(const KeypressCommand& command)
{
    EncodedValue encodedValue = EncodedValue::createObject();

    ENCODE_TYPE_WITH_KEY(encodedValue, String, commandName, command.commandName);
    ENCODE_OPTIONAL_TYPE_WITH_KEY(encodedValue, String, text, command.text, !command.text.isEmpty());

    return encodedValue;
}

bool EncodingTraits<KeypressCommand>::decodeValue(EncodedValue& encodedValue, KeypressCommand& decodedValue)
{
    DECODE_TYPE_WITH_KEY(encodedValue, String, commandName);
    DECODE_OPTIONAL_TYPE_WITH_KEY(encodedValue, String, text);

    decodedValue = textWasDecoded ? KeypressCommand(commandName, text) : KeypressCommand(commandName);
    return true;
}

class PlatformKeyboardEventAppKit : public WebCore::PlatformKeyboardEvent {
public:
    PlatformKeyboardEventAppKit(const PlatformKeyboardEvent& event, bool handledByInputMethod, Vector<KeypressCommand>& commands)
        : PlatformKeyboardEvent(event)
    {
        m_handledByInputMethod = handledByInputMethod;
        m_commands = commands;
    }
};
#endif // USE(APPKIT)

EncodedValue EncodingTraits<PlatformKeyboardEvent>::encodeValue(const PlatformKeyboardEvent& input)
{
    EncodedValue encodedValue = EncodedValue::createObject();

    ENCODE_TYPE_WITH_KEY(encodedValue, double, timestamp, input.timestamp());
    ENCODE_TYPE_WITH_KEY(encodedValue, PlatformEvent::Type, type, input.type());
    ENCODE_TYPE_WITH_KEY(encodedValue, PlatformEvent::Modifiers, modifiers, static_cast<PlatformEvent::Modifiers>(input.modifiers()));
    ENCODE_TYPE_WITH_KEY(encodedValue, String, text, input.text());
    ENCODE_TYPE_WITH_KEY(encodedValue, String, unmodifiedText, input.unmodifiedText());
    ENCODE_TYPE_WITH_KEY(encodedValue, String, keyIdentifier, input.keyIdentifier());
    ENCODE_TYPE_WITH_KEY(encodedValue, int, windowsVirtualKeyCode, input.windowsVirtualKeyCode());
    ENCODE_TYPE_WITH_KEY(encodedValue, int, nativeVirtualKeyCode, input.nativeVirtualKeyCode());
    ENCODE_TYPE_WITH_KEY(encodedValue, int, macCharCode, input.macCharCode());
    ENCODE_TYPE_WITH_KEY(encodedValue, bool, autoRepeat, input.isAutoRepeat());
    ENCODE_TYPE_WITH_KEY(encodedValue, bool, keypad, input.isKeypad());
    ENCODE_TYPE_WITH_KEY(encodedValue, bool, systemKey, input.isSystemKey());
#if USE(APPKIT)
    ENCODE_TYPE_WITH_KEY(encodedValue, bool, handledByInputMethod, input.handledByInputMethod());
    ENCODE_TYPE_WITH_KEY(encodedValue, Vector<KeypressCommand>, commands, input.commands());
#endif
    return encodedValue;
}

bool EncodingTraits<PlatformKeyboardEvent>::decodeValue(EncodedValue& encodedValue, std::unique_ptr<PlatformKeyboardEvent>& input)
{
    DECODE_TYPE_WITH_KEY(encodedValue, double, timestamp);
    DECODE_TYPE_WITH_KEY(encodedValue, PlatformEvent::Type, type);
    DECODE_TYPE_WITH_KEY(encodedValue, PlatformEvent::Modifiers, modifiers);
    DECODE_TYPE_WITH_KEY(encodedValue, String, text);
    DECODE_TYPE_WITH_KEY(encodedValue, String, unmodifiedText);
    DECODE_TYPE_WITH_KEY(encodedValue, String, keyIdentifier);
    DECODE_TYPE_WITH_KEY(encodedValue, int, windowsVirtualKeyCode);
    DECODE_TYPE_WITH_KEY(encodedValue, int, nativeVirtualKeyCode);
    DECODE_TYPE_WITH_KEY(encodedValue, int, macCharCode);
    DECODE_TYPE_WITH_KEY(encodedValue, bool, autoRepeat);
    DECODE_TYPE_WITH_KEY(encodedValue, bool, keypad);
    DECODE_TYPE_WITH_KEY(encodedValue, bool, systemKey);
#if USE(APPKIT)
    DECODE_TYPE_WITH_KEY(encodedValue, bool, handledByInputMethod);
    DECODE_TYPE_WITH_KEY(encodedValue, Vector<KeypressCommand>, commands);
#endif

    PlatformKeyboardEvent platformEvent = PlatformKeyboardEvent(type, text, unmodifiedText, keyIdentifier, windowsVirtualKeyCode, nativeVirtualKeyCode, macCharCode, autoRepeat, keypad, systemKey, modifiers, timestamp);
#if USE(APPKIT)
    input = std::make_unique<PlatformKeyboardEventAppKit>(platformEvent, handledByInputMethod, commands);
#else
    input = std::make_unique<PlatformKeyboardEvent>(platformEvent);
#endif
    return true;
}

EncodedValue EncodingTraits<PlatformMouseEvent>::encodeValue(const PlatformMouseEvent& input)
{
    EncodedValue encodedValue = EncodedValue::createObject();

    ENCODE_TYPE_WITH_KEY(encodedValue, int, positionX, input.position().x());
    ENCODE_TYPE_WITH_KEY(encodedValue, int, positionY, input.position().y());
    ENCODE_TYPE_WITH_KEY(encodedValue, int, globalPositionX, input.globalPosition().x());
    ENCODE_TYPE_WITH_KEY(encodedValue, int, globalPositionY, input.globalPosition().y());
    ENCODE_TYPE_WITH_KEY(encodedValue, MouseButton, button, input.button());
    ENCODE_TYPE_WITH_KEY(encodedValue, PlatformEvent::Type, type, input.type());
    ENCODE_TYPE_WITH_KEY(encodedValue, int, clickCount, input.clickCount());
    ENCODE_TYPE_WITH_KEY(encodedValue, bool, shiftKey, input.shiftKey());
    ENCODE_TYPE_WITH_KEY(encodedValue, bool, ctrlKey, input.ctrlKey());
    ENCODE_TYPE_WITH_KEY(encodedValue, bool, altKey, input.altKey());
    ENCODE_TYPE_WITH_KEY(encodedValue, bool, metaKey, input.metaKey());
    ENCODE_TYPE_WITH_KEY(encodedValue, int, timestamp, input.timestamp());

    return encodedValue;
}

bool EncodingTraits<PlatformMouseEvent>::decodeValue(EncodedValue& encodedValue, std::unique_ptr<PlatformMouseEvent>& input)
{
    DECODE_TYPE_WITH_KEY(encodedValue, int, positionX);
    DECODE_TYPE_WITH_KEY(encodedValue, int, positionY);
    DECODE_TYPE_WITH_KEY(encodedValue, int, globalPositionX);
    DECODE_TYPE_WITH_KEY(encodedValue, int, globalPositionY);
    DECODE_TYPE_WITH_KEY(encodedValue, MouseButton, button);
    DECODE_TYPE_WITH_KEY(encodedValue, PlatformEvent::Type, type);
    DECODE_TYPE_WITH_KEY(encodedValue, int, clickCount);
    DECODE_TYPE_WITH_KEY(encodedValue, bool, shiftKey);
    DECODE_TYPE_WITH_KEY(encodedValue, bool, ctrlKey);
    DECODE_TYPE_WITH_KEY(encodedValue, bool, altKey);
    DECODE_TYPE_WITH_KEY(encodedValue, bool, metaKey);
    DECODE_TYPE_WITH_KEY(encodedValue, int, timestamp);

    input = std::make_unique<PlatformMouseEvent>(IntPoint(positionX, positionY),
        IntPoint(globalPositionX, globalPositionY),
        button, type, clickCount,
        shiftKey, ctrlKey, altKey, metaKey, timestamp);
    return true;
}

#if PLATFORM(COCOA)
struct PlatformWheelEventCocoaArguments {
    bool directionInvertedFromDevice;
    bool hasPreciseScrollingDeltas;
    PlatformWheelEventPhase phase;
    PlatformWheelEventPhase momentumPhase;
    int scrollCount;
    float unacceleratedScrollingDeltaX;
    float unacceleratedScrollingDeltaY;
};

class PlatformWheelEventCocoa : public PlatformWheelEvent {
public:
    PlatformWheelEventCocoa(PlatformWheelEvent& event, PlatformWheelEventCocoaArguments& arguments)
        : PlatformWheelEvent(event)
    {
        m_directionInvertedFromDevice = arguments.directionInvertedFromDevice;
        m_hasPreciseScrollingDeltas = arguments.hasPreciseScrollingDeltas;
        m_phase = arguments.phase;
        m_momentumPhase = arguments.momentumPhase;
        m_scrollCount = arguments.scrollCount;
        m_unacceleratedScrollingDeltaX = arguments.unacceleratedScrollingDeltaX;
        m_unacceleratedScrollingDeltaY = arguments.unacceleratedScrollingDeltaY;
    }
};
#endif // PLATFORM(COCOA)

EncodedValue EncodingTraits<PlatformWheelEvent>::encodeValue(const PlatformWheelEvent& input)
{
    EncodedValue encodedData = EncodedValue::createObject();

    ENCODE_TYPE_WITH_KEY(encodedData, int, positionX, input.position().x());
    ENCODE_TYPE_WITH_KEY(encodedData, int, positionY, input.position().y());
    ENCODE_TYPE_WITH_KEY(encodedData, int, globalPositionX, input.globalPosition().x());
    ENCODE_TYPE_WITH_KEY(encodedData, int, globalPositionY, input.globalPosition().y());
    ENCODE_TYPE_WITH_KEY(encodedData, bool, shiftKey, input.shiftKey());
    ENCODE_TYPE_WITH_KEY(encodedData, bool, ctrlKey, input.ctrlKey());
    ENCODE_TYPE_WITH_KEY(encodedData, bool, altKey, input.altKey());
    ENCODE_TYPE_WITH_KEY(encodedData, bool, metaKey, input.metaKey());
    ENCODE_TYPE_WITH_KEY(encodedData, float, deltaX, input.deltaX());
    ENCODE_TYPE_WITH_KEY(encodedData, float, deltaY, input.deltaY());
    ENCODE_TYPE_WITH_KEY(encodedData, float, wheelTicksX, input.wheelTicksX());
    ENCODE_TYPE_WITH_KEY(encodedData, float, wheelTicksY, input.wheelTicksY());
    ENCODE_TYPE_WITH_KEY(encodedData, PlatformWheelEventGranularity, granularity, static_cast<PlatformWheelEventGranularity>(input.granularity()));

#if PLATFORM(COCOA)
    ENCODE_TYPE_WITH_KEY(encodedData, bool, directionInvertedFromDevice, input.directionInvertedFromDevice());
    ENCODE_TYPE_WITH_KEY(encodedData, bool, hasPreciseScrollingDeltas, input.hasPreciseScrollingDeltas());
    ENCODE_TYPE_WITH_KEY(encodedData, PlatformWheelEventPhase, phase, static_cast<PlatformWheelEventPhase>(input.phase()));
    ENCODE_TYPE_WITH_KEY(encodedData, PlatformWheelEventPhase, momentumPhase, static_cast<PlatformWheelEventPhase>(input.momentumPhase()));
    ENCODE_TYPE_WITH_KEY(encodedData, int, scrollCount, input.scrollCount());
    ENCODE_TYPE_WITH_KEY(encodedData, float, unacceleratedScrollingDeltaX, input.unacceleratedScrollingDeltaX());
    ENCODE_TYPE_WITH_KEY(encodedData, float, unacceleratedScrollingDeltaY, input.unacceleratedScrollingDeltaY());
#endif

    return encodedData;
}

bool EncodingTraits<PlatformWheelEvent>::decodeValue(EncodedValue& encodedData, std::unique_ptr<PlatformWheelEvent>& input)
{
    DECODE_TYPE_WITH_KEY(encodedData, int, positionX);
    DECODE_TYPE_WITH_KEY(encodedData, int, positionY);
    DECODE_TYPE_WITH_KEY(encodedData, int, globalPositionX);
    DECODE_TYPE_WITH_KEY(encodedData, int, globalPositionY);
    DECODE_TYPE_WITH_KEY(encodedData, bool, shiftKey);
    DECODE_TYPE_WITH_KEY(encodedData, bool, ctrlKey);
    DECODE_TYPE_WITH_KEY(encodedData, bool, altKey);
    DECODE_TYPE_WITH_KEY(encodedData, bool, metaKey);
    DECODE_TYPE_WITH_KEY(encodedData, float, deltaX);
    DECODE_TYPE_WITH_KEY(encodedData, float, deltaY);
    DECODE_TYPE_WITH_KEY(encodedData, float, wheelTicksX);
    DECODE_TYPE_WITH_KEY(encodedData, float, wheelTicksY);
    DECODE_TYPE_WITH_KEY(encodedData, PlatformWheelEventGranularity, granularity);

#if PLATFORM(COCOA)
    PlatformWheelEventCocoaArguments arguments;
    DECODE_TYPE_WITH_KEY_TO_LVALUE(encodedData, bool, directionInvertedFromDevice, arguments.directionInvertedFromDevice);
    DECODE_TYPE_WITH_KEY_TO_LVALUE(encodedData, bool, hasPreciseScrollingDeltas, arguments.hasPreciseScrollingDeltas);
    DECODE_TYPE_WITH_KEY_TO_LVALUE(encodedData, PlatformWheelEventPhase, phase, arguments.phase);
    DECODE_TYPE_WITH_KEY_TO_LVALUE(encodedData, PlatformWheelEventPhase, momentumPhase, arguments.momentumPhase);
    DECODE_TYPE_WITH_KEY_TO_LVALUE(encodedData, int, scrollCount, arguments.scrollCount);
    DECODE_TYPE_WITH_KEY_TO_LVALUE(encodedData, float, unacceleratedScrollingDeltaX, arguments.unacceleratedScrollingDeltaX);
    DECODE_TYPE_WITH_KEY_TO_LVALUE(encodedData, float, unacceleratedScrollingDeltaY, arguments.unacceleratedScrollingDeltaY);
#endif

    PlatformWheelEvent event(IntPoint(positionX, positionY), IntPoint(globalPositionX, globalPositionY),
        deltaX, deltaY, wheelTicksX, wheelTicksY, granularity, shiftKey, ctrlKey, altKey, metaKey);

#if PLATFORM(COCOA)
    input = std::make_unique<PlatformWheelEventCocoa>(event, arguments);
#else
    input = std::make_unique<PlatformWheelEvent>(event);
#endif
    return true;
}

EncodedValue EncodingTraits<PluginData>::encodeValue(RefPtr<PluginData> input)
{
    EncodedValue encodedData = EncodedValue::createObject();

    ENCODE_TYPE_WITH_KEY(encodedData, Vector<PluginInfo>, plugins, input->plugins());
    ENCODE_TYPE_WITH_KEY(encodedData, Vector<MimeClassInfo>, mimes, input->mimes());
    ENCODE_TYPE_WITH_KEY(encodedData, Vector<size_t>, mimePluginIndices, input->mimePluginIndices());

    return encodedData;
}

class DeserializedPluginData : public PluginData {
public:
    DeserializedPluginData(Vector<PluginInfo> plugins, Vector<MimeClassInfo> mimes, Vector<size_t> indices)
        : PluginData(plugins, mimes, indices)
    {
    }
};

bool EncodingTraits<PluginData>::decodeValue(EncodedValue& encodedData, RefPtr<PluginData>& input)
{
    DECODE_TYPE_WITH_KEY(encodedData, Vector<PluginInfo>, plugins);
    DECODE_TYPE_WITH_KEY(encodedData, Vector<MimeClassInfo>, mimes);
    DECODE_TYPE_WITH_KEY(encodedData, Vector<size_t>, mimePluginIndices);

    input = adoptRef(new DeserializedPluginData(plugins, mimes, mimePluginIndices));

    return true;
}

template<>
EncodedValue EncodingTraits<PluginInfo>::encodeValue(const PluginInfo& input)
{
    EncodedValue encodedData = EncodedValue::createObject();

    ENCODE_TYPE_WITH_KEY(encodedData, String, name, input.name);
    ENCODE_TYPE_WITH_KEY(encodedData, String, file, input.file);
    ENCODE_TYPE_WITH_KEY(encodedData, String, desc, input.desc);
    ENCODE_TYPE_WITH_KEY(encodedData, Vector<MimeClassInfo>, mimes, input.mimes);
    ENCODE_TYPE_WITH_KEY(encodedData, bool, isApplicationPlugin, input.isApplicationPlugin);

    return encodedData;
}

template<>
bool EncodingTraits<PluginInfo>::decodeValue(EncodedValue& encodedData, PluginInfo& input)
{
    PluginInfo info;

    DECODE_TYPE_WITH_KEY_TO_LVALUE(encodedData, String, name, info.name);
    DECODE_TYPE_WITH_KEY_TO_LVALUE(encodedData, String, file, info.file);
    DECODE_TYPE_WITH_KEY_TO_LVALUE(encodedData, String, desc, info.desc);
    DECODE_TYPE_WITH_KEY_TO_LVALUE(encodedData, Vector<MimeClassInfo>, mimes, info.mimes);
    DECODE_TYPE_WITH_KEY_TO_LVALUE(encodedData, bool, isApplicationPlugin, info.isApplicationPlugin);

    input = info;
    return true;
}

EncodedValue EncodingTraits<SecurityOrigin>::encodeValue(RefPtr<SecurityOrigin> input)
{
    return EncodedValue::createString(input->toString());
}

bool EncodingTraits<SecurityOrigin>::decodeValue(EncodedValue& encodedValue, RefPtr<SecurityOrigin>& input)
{
    input = SecurityOrigin::createFromString(encodedValue.convertTo<String>());
    return true;
}

EncodedValue EncodingTraits<URL>::encodeValue(const URL& input)
{
    return EncodedValue::createString(input.string());
}

bool EncodingTraits<URL>::decodeValue(EncodedValue& encodedValue, URL& input)
{
    input = URL(WebCore::ParsedURLString, encodedValue.convertTo<String>());
    return true;
}

} // namespace JSC

#endif // ENABLE(WEB_REPLAY)
