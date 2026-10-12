/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "config.h"
#include "BidiInputAgent.h"

#if ENABLE(WEBDRIVER_BIDI)

#include "AutomationProtocolObjects.h"
#include "WebAutomationSession.h"
#include "WebAutomationSessionMacros.h"
#include "WebDriverBidiProtocolObjects.h"
#include <numbers>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/TextBreakIterator.h>

namespace WebKit::BidiInput {

WTF_MAKE_TZONE_ALLOCATED_IMPL(State);

const Source* State::find(const String& id) const
{
    auto it = m_inputStateMap.find(id);
    return it == m_inputStateMap.end() ? nullptr : &it->value;
}

void State::add(const String& id, Source source)
{
    m_inputStateMap.set(id, source);
}

#if ENABLE(WEBDRIVER_ACTIONS_API)
static SimulatedInputSourceType simulatedSourceType(const Source& source)
{
    auto type = SimulatedInputSourceType::Null;
    switch (source.type) {
    case SourceType::None:
        type = SimulatedInputSourceType::Null;
        break;
    case SourceType::Key:
        type = SimulatedInputSourceType::Keyboard;
        break;
    case SourceType::Wheel:
        type = SimulatedInputSourceType::Wheel;
        break;
    case SourceType::Pointer:
        switch (source.pointerType) {
        case PointerType::Mouse:
            type = SimulatedInputSourceType::Mouse;
            break;
        case PointerType::Pen:
            type = SimulatedInputSourceType::Pen;
            break;
        case PointerType::Touch:
            type = SimulatedInputSourceType::Touch;
            break;
        }
        break;
    }

#if !ENABLE(WEBDRIVER_MOUSE_INTERACTIONS) && ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)
    if (type == SimulatedInputSourceType::Mouse || type == SimulatedInputSourceType::Pen)
        type = SimulatedInputSourceType::Touch;
#elif ENABLE(WEBDRIVER_MOUSE_INTERACTIONS) && !ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)
    if (type == SimulatedInputSourceType::Touch)
        type = SimulatedInputSourceType::Mouse;
#endif

    return type;
}

SimulatedInputSource& State::simulatedSource(const Action& action)
{
    auto* source = find(action.id);
    if (!source) {
        add(action.id, { action.sourceType, action.pointerType });
        source = find(action.id);
    }

    return m_simulatedSources.ensure(action.id, [&] {
        return SimulatedInputSource::create(simulatedSourceType(*source));
    }).iterator->value.get();
}
#endif

void State::enqueue(QueuedCommand&& command)
{
    m_queue.append(WTF::move(command));
    if (!m_isRunningQueuedCommand)
        runNextQueuedCommand();
}

void State::runNextQueuedCommand()
{
    if (m_isPumpingQueuedCommands) {
        m_shouldPumpAgain = true;
        return;
    }

    m_isPumpingQueuedCommands = true;
    do {
        m_shouldPumpAgain = false;
        if (m_queue.isEmpty()) {
            m_isRunningQueuedCommand = false;
            break;
        }

        m_isRunningQueuedCommand = true;
        auto command = m_queue.takeFirst();
        command([weakThis = WeakPtr { *this }] {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->runNextQueuedCommand();
        });
    } while (m_shouldPumpAgain);
    m_isPumpingQueuedCommands = false;
}

void State::reset()
{
    m_inputStateMap.clear();
#if ENABLE(WEBDRIVER_ACTIONS_API)
    m_simulatedSources.clear();
#endif
    m_inputCancelList.clear();
}

static constexpr double maxSafeInteger = 9007199254740991.0;

static std::optional<double> numberValue(const JSON::Value& value)
{
    if (value.type() != JSON::Value::Type::Double && value.type() != JSON::Value::Type::Integer)
        return std::nullopt;

    return value.asDouble();
}

static std::optional<double> integerValue(const JSON::Value& value, double min, double max)
{
    auto number = numberValue(value);
    if (!number || std::trunc(*number) != *number || *number < min || *number > max)
        return std::nullopt;

    return number;
}

// Returns: nullopt = property absent; error string = invalid; value otherwise.
template<typename T> using PropertyResult = std::expected<std::optional<T>, String>;

static PropertyResult<uint64_t> optionalUnsigned(const JSON::Object& object, const String& name, double max = maxSafeInteger)
{
    RefPtr value = object.getValue(name);
    if (!value)
        return std::optional<uint64_t> { };

    auto integer = integerValue(*value, 0, max);
    if (!integer)
        return makeUnexpected(makeString("'"_s, name, "' must be an integer between 0 and "_s, max));

    return std::optional<uint64_t> { static_cast<uint64_t>(*integer) };
}

static PropertyResult<double> optionalNumberInRange(const JSON::Object& object, const String& name, double min, double max)
{
    RefPtr value = object.getValue(name);
    if (!value)
        return std::optional<double> { };

    auto number = numberValue(*value);
    if (!number || *number < min || *number > max)
        return makeUnexpected(makeString("'"_s, name, "' must be a number in range"_s));

    return std::optional<double> { *number };
}

static PropertyResult<int> optionalIntegerInRange(const JSON::Object& object, const String& name, int min, int max)
{
    RefPtr value = object.getValue(name);
    if (!value)
        return std::optional<int> { };

    auto integer = integerValue(*value, min, max);
    if (!integer)
        return makeUnexpected(makeString("'"_s, name, "' must be an integer in range"_s));

    return std::optional<int> { static_cast<int>(*integer) };
}

// https://w3c.github.io/webdriver/#dfn-process-a-pause-action
static std::expected<void, String> processPauseAction(const JSON::Object& item, Action& action)
{
    auto duration = optionalUnsigned(item, "duration"_s);
    if (!duration)
        return makeUnexpected(duration.error());

    action.duration = *duration;
    return { };
}

static bool isSingleGraphemeCluster(const String& string)
{
    if (string.isEmpty())
        return false;
    return WTF::numGraphemeClusters(string) == 1;
}

// is input.ElementOrigin (BiDi §7.9.2.1): { type: "element", element: script.SharedReference }.
static std::optional<String> elementOriginSharedId(const JSON::Value& origin)
{
    RefPtr object = origin.asObject();
    if (!object || object->getString("type"_s) != "element"_s)
        return std::nullopt;

    RefPtr element = object->getObject("element"_s);
    if (!element)
        return std::nullopt;

    RefPtr sharedId = element->getValue("sharedId"_s);
    if (!sharedId || sharedId->type() != JSON::Value::Type::String)
        return std::nullopt;

    RefPtr handle = element->getValue("handle"_s);
    if (handle && handle->type() != JSON::Value::Type::String)
        return std::nullopt;

    return sharedId->asString();
}

static std::expected<void, String> processOrigin(const JSON::Object& item, Action& action, bool allowPointerOrigin)
{
    RefPtr origin = item.getValue("origin"_s);
    if (!origin) {
        action.origin = OriginType::Viewport;
        return { };
    }

    if (origin->type() == JSON::Value::Type::String) {
        auto string = origin->asString();
        if (string == "viewport"_s) {
            action.origin = OriginType::Viewport;
            return { };
        }

        if (string == "pointer"_s && allowPointerOrigin) {
            action.origin = OriginType::Pointer;
            return { };
        }

        return makeUnexpected("'origin' must be 'viewport', 'pointer', or an input.ElementOrigin"_s);
    }

    auto sharedId = elementOriginSharedId(*origin);
    if (!sharedId)
        return makeUnexpected("'origin' is not a valid input.ElementOrigin"_s);

    action.origin = OriginType::Element;
    action.originSharedId = *sharedId;
    return { };
}

// https://w3c.github.io/webdriver/#dfn-process-a-pointer-move-action
// The classic spec validates each property independently.
static std::expected<void, String> processPointerProperties(const JSON::Object& item, Action& action)
{
    auto& p = action.properties;

    auto width = optionalUnsigned(item, "width"_s);
    if (!width)
        return makeUnexpected(width.error());
    p.width = *width;

    auto height = optionalUnsigned(item, "height"_s);
    if (!height)
        return makeUnexpected(height.error());
    p.height = *height;

    auto pressure = optionalNumberInRange(item, "pressure"_s, 0, 1);
    if (!pressure)
        return makeUnexpected(pressure.error());
    p.pressure = *pressure;

    auto tangentialPressure = optionalNumberInRange(item, "tangentialPressure"_s, -1, 1);
    if (!tangentialPressure)
        return makeUnexpected(tangentialPressure.error());
    p.tangentialPressure = *tangentialPressure;

    auto tiltX = optionalIntegerInRange(item, "tiltX"_s, -90, 90);
    if (!tiltX)
        return makeUnexpected(tiltX.error());
    p.tiltX = *tiltX;

    auto tiltY = optionalIntegerInRange(item, "tiltY"_s, -90, 90);
    if (!tiltY)
        return makeUnexpected(tiltY.error());
    p.tiltY = *tiltY;

    auto twist = optionalIntegerInRange(item, "twist"_s, 0, 359);
    if (!twist)
        return makeUnexpected(twist.error());
    p.twist = *twist;

    auto altitudeAngle = optionalNumberInRange(item, "altitudeAngle"_s, 0, std::numbers::pi / 2);
    if (!altitudeAngle)
        return makeUnexpected(altitudeAngle.error());
    p.altitudeAngle = *altitudeAngle;

    auto azimuthAngle = optionalNumberInRange(item, "azimuthAngle"_s, 0, 2 * std::numbers::pi);
    if (!azimuthAngle)
        return makeUnexpected(azimuthAngle.error());
    p.azimuthAngle = *azimuthAngle;

    return { };
}

static std::expected<void, String> processButton(const JSON::Object& item, Action& action)
{
    RefPtr value = item.getValue("button"_s);
    auto button = value ? integerValue(*value, 0, maxSafeInteger) : std::nullopt;
    if (!button)
        return makeUnexpected("'button' must be an integer between 0 and 2^53-1"_s);

    action.button = static_cast<uint64_t>(*button);
    return { };
}

static std::expected<void, String> processPointerAction(const JSON::Object& item, Action& action)
{
    auto subtype = item.getString("type"_s);
    if (subtype == "pause"_s) {
        action.type = ActionType::Pause;
        return processPauseAction(item, action);
    }

    if (subtype == "pointerDown"_s || subtype == "pointerUp"_s) {
        action.type = subtype == "pointerDown"_s ? ActionType::PointerDown : ActionType::PointerUp;
        if (auto result = processButton(item, action); !result)
            return result;
        return processPointerProperties(item, action);
    }

    if (subtype == "pointerMove"_s) {
        action.type = ActionType::PointerMove;

        auto duration = optionalUnsigned(item, "duration"_s);
        if (!duration)
            return makeUnexpected(duration.error());
        action.duration = *duration;

        if (auto result = processOrigin(item, action, /* allowPointerOrigin */ true); !result)
            return result;

        RefPtr x = item.getValue("x"_s);
        RefPtr y = item.getValue("y"_s);
        auto xNumber = x ? numberValue(*x) : std::nullopt;
        auto yNumber = y ? numberValue(*y) : std::nullopt;
        if (!xNumber || !yNumber)
            return makeUnexpected("'x' and 'y' must be numbers"_s);
        action.x = *xNumber;
        action.y = *yNumber;

        return processPointerProperties(item, action);
    }

    if (subtype == "pointerCancel"_s) {
        action.type = ActionType::PointerCancel;
        return { };
    }

    return makeUnexpected("Unknown pointer action type"_s);
}

static std::expected<void, String> processWheelAction(const JSON::Object& item, Action& action)
{
    auto subtype = item.getString("type"_s);
    if (subtype == "pause"_s) {
        action.type = ActionType::Pause;
        return processPauseAction(item, action);
    }

    if (subtype != "scroll"_s)
        return makeUnexpected("Unknown wheel action type"_s);

    action.type = ActionType::Scroll;

    auto duration = optionalUnsigned(item, "duration"_s);
    if (!duration)
        return makeUnexpected(duration.error());
    action.duration = *duration;

    // Wheel sources only support "viewport" or an element origin; "pointer" is rejected.
    if (auto result = processOrigin(item, action, /* allowPointerOrigin */ false); !result)
        return result;

    auto integerProperty = [&](ASCIILiteral name) -> std::optional<int64_t> {
        RefPtr value = item.getValue(String { name });
        auto integer = value ? integerValue(*value, -maxSafeInteger, maxSafeInteger) : std::nullopt;
        return integer ? std::optional<int64_t> { static_cast<int64_t>(*integer) } : std::nullopt;
    };

    auto x = integerProperty("x"_s);
    auto y = integerProperty("y"_s);
    auto deltaX = integerProperty("deltaX"_s);
    auto deltaY = integerProperty("deltaY"_s);
    if (!x || !y || !deltaX || !deltaY)
        return makeUnexpected("'x', 'y', 'deltaX' and 'deltaY' must be integers"_s);

    action.x = *x;
    action.y = *y;
    action.deltaX = *deltaX;
    action.deltaY = *deltaY;
    return { };
}

static std::expected<void, String> processKeyAction(const JSON::Object& item, Action& action)
{
    auto subtype = item.getString("type"_s);
    if (subtype == "pause"_s) {
        action.type = ActionType::Pause;
        return processPauseAction(item, action);
    }

    if (subtype != "keyDown"_s && subtype != "keyUp"_s)
        return makeUnexpected("Unknown key action type"_s);

    action.type = subtype == "keyDown"_s ? ActionType::KeyDown : ActionType::KeyUp;

    RefPtr value = item.getValue("value"_s);
    // The spec currently leaves open whether "value" must be a single Unicode code point or a
    // single grapheme cluster (open editorial issue on dfn-process-a-key-action). We follow the
    // grapheme-cluster interpretation already used for classic Automation (see
    // WebAutomationSession.cpp's use of WTF::numGraphemeClusters for pressedCharKey).
    if (!value || value->type() != JSON::Value::Type::String || !isSingleGraphemeCluster(value->asString()))
        return makeUnexpected("'value' must be a single grapheme cluster"_s);

    action.value = value->asString();
    return { };
}

static std::expected<void, String> processNullAction(const JSON::Object& item, Action& action)
{
    if (item.getString("type"_s) != "pause"_s)
        return makeUnexpected("Null input sources only support 'pause'"_s);

    action.type = ActionType::Pause;
    return processPauseAction(item, action);
}

// https://w3c.github.io/webdriver/#dfn-process-an-input-source-action-sequence
static std::expected<Vector<Action>, String> processInputSourceActionSequence(State& state, const JSON::Value& sequenceValue)
{
    RefPtr sequence = sequenceValue.asObject();
    if (!sequence)
        return makeUnexpected("Each input source must be an object"_s);

    RefPtr typeValue = sequence->getValue("type"_s);
    if (!typeValue || typeValue->type() != JSON::Value::Type::String)
        return makeUnexpected("'type' must be a string"_s);
    auto typeString = typeValue->asString();
    SourceType type;
    if (typeString == "none"_s)
        type = SourceType::None;
    else if (typeString == "key"_s)
        type = SourceType::Key;
    else if (typeString == "pointer"_s)
        type = SourceType::Pointer;
    else if (typeString == "wheel"_s)
        type = SourceType::Wheel;
    else
        return makeUnexpected("'type' must be one of 'none', 'key', 'pointer', 'wheel'"_s);

    RefPtr idValue = sequence->getValue("id"_s);
    if (!idValue || idValue->type() != JSON::Value::Type::String)
        return makeUnexpected("'id' must be a string"_s);
    auto id = idValue->asString();

    PointerType pointerType = PointerType::Mouse;
    if (type == SourceType::Pointer) {
        // https://w3c.github.io/webdriver/#dfn-process-pointer-parameters
        if (RefPtr parameters = sequence->getValue("parameters"_s)) {
            RefPtr parametersObject = parameters->asObject();
            if (!parametersObject)
                return makeUnexpected("'parameters' must be an object"_s);

            if (RefPtr pointerTypeValue = parametersObject->getValue("pointerType"_s)) {
                if (pointerTypeValue->type() != JSON::Value::Type::String)
                    return makeUnexpected("'pointerType' must be a string"_s);

                auto value = pointerTypeValue->asString();
                if (value == "mouse"_s)
                    pointerType = PointerType::Mouse;
                else if (value == "pen"_s)
                    pointerType = PointerType::Pen;
                else if (value == "touch"_s)
                    pointerType = PointerType::Touch;
                else
                    return makeUnexpected("'pointerType' must be 'mouse', 'pen' or 'touch'"_s);
            }
        }
    }

    // https://w3c.github.io/webdriver/#dfn-get-or-create-an-input-source
    if (auto* existing = state.find(id)) {
        if (existing->type != type)
            return makeUnexpected("An input source with this id already exists with a different type"_s);

        if (type == SourceType::Pointer && existing->pointerType != pointerType)
            return makeUnexpected("A pointer input source with this id already exists with a different pointerType"_s);
    } else
        state.add(id, { type, pointerType });

    RefPtr actionsValue = sequence->getValue("actions"_s);
    RefPtr actionItems = actionsValue ? actionsValue->asArray() : nullptr;
    if (!actionItems)
        return makeUnexpected("'actions' must be an array"_s);

    Vector<Action> actions;
    actions.reserveInitialCapacity(actionItems->length());
    for (auto& actionItemValue : *actionItems) {
        RefPtr actionItem = actionItemValue->asObject();
        if (!actionItem)
            return makeUnexpected("Each action must be an object"_s);

        Action action;
        action.id = id;
        action.sourceType = type;
        action.pointerType = pointerType;

        std::expected<void, String> result;
        switch (type) {
        case SourceType::None: result = processNullAction(*actionItem, action); break;
        case SourceType::Key: result = processKeyAction(*actionItem, action); break;
        case SourceType::Pointer: result = processPointerAction(*actionItem, action); break;
        case SourceType::Wheel: result = processWheelAction(*actionItem, action); break;
        }
        if (!result)
            return makeUnexpected(result.error());

        actions.append(WTF::move(action));
    }

    return actions;
}

static std::expected<ActionsByTick, String> extractActionSequence(State& state, const JSON::Array& actions)
{
    ActionsByTick actionsByTick;
    for (auto& sequence : actions) {
        auto sourceActions = processInputSourceActionSequence(state, sequence.get());
        if (!sourceActions)
            return makeUnexpected(sourceActions.error());

        for (size_t i = 0; i < sourceActions->size(); ++i) {
            if (actionsByTick.size() <= i)
                actionsByTick.constructAndAppend();
            actionsByTick[i].append(WTF::move(sourceActions->at(i)));
        }
    }

    return actionsByTick;
}

#if ENABLE(WEBDRIVER_ACTIONS_API)

// https://w3c.github.io/webdriver/#keyboard-actions
static std::optional<VirtualKey> virtualKeyForKeyValue(const String& value)
{
    if (value.length() != 1)
        return std::nullopt;

    switch (value[0]) {
    case 0xE001: return VirtualKey::Cancel;
    case 0xE002: return VirtualKey::Help;
    case 0xE003: return VirtualKey::Backspace;
    case 0xE004: return VirtualKey::Tab;
    case 0xE005: return VirtualKey::Clear;
    case 0xE006: return VirtualKey::Return;
    case 0xE007: return VirtualKey::Enter;
    case 0xE008: return VirtualKey::Shift;
    case 0xE050: return VirtualKey::ShiftRight;
    case 0xE009: return VirtualKey::Control;
    case 0xE051: return VirtualKey::ControlRight;
    case 0xE00A: return VirtualKey::Alternate;
    case 0xE052: return VirtualKey::AlternateRight;
    case 0xE00B: return VirtualKey::Pause;
    case 0xE00C: return VirtualKey::Escape;
    case 0xE00D: return VirtualKey::Space;
    case 0xE00E: return VirtualKey::PageUp;
    case 0xE054: return VirtualKey::PageUpRight;
    case 0xE00F: return VirtualKey::PageDown;
    case 0xE055: return VirtualKey::PageDownRight;
    case 0xE010: return VirtualKey::End;
    case 0xE056: return VirtualKey::EndRight;
    case 0xE011: return VirtualKey::Home;
    case 0xE057: return VirtualKey::HomeRight;
    case 0xE012: return VirtualKey::LeftArrow;
    case 0xE058: return VirtualKey::LeftArrowRight;
    case 0xE013: return VirtualKey::UpArrow;
    case 0xE059: return VirtualKey::UpArrowRight;
    case 0xE014: return VirtualKey::RightArrow;
    case 0xE05A: return VirtualKey::RightArrowRight;
    case 0xE015: return VirtualKey::DownArrow;
    case 0xE05B: return VirtualKey::DownArrowRight;
    case 0xE016: return VirtualKey::Insert;
    case 0xE05C: return VirtualKey::InsertRight;
    case 0xE017: return VirtualKey::Delete;
    case 0xE05D: return VirtualKey::DeleteRight;
    case 0xE018: return VirtualKey::Semicolon;
    case 0xE019: return VirtualKey::Equals;
    case 0xE01A: return VirtualKey::NumberPad0;
    case 0xE01B: return VirtualKey::NumberPad1;
    case 0xE01C: return VirtualKey::NumberPad2;
    case 0xE01D: return VirtualKey::NumberPad3;
    case 0xE01E: return VirtualKey::NumberPad4;
    case 0xE01F: return VirtualKey::NumberPad5;
    case 0xE020: return VirtualKey::NumberPad6;
    case 0xE021: return VirtualKey::NumberPad7;
    case 0xE022: return VirtualKey::NumberPad8;
    case 0xE023: return VirtualKey::NumberPad9;
    case 0xE024: return VirtualKey::NumberPadMultiply;
    case 0xE025: return VirtualKey::NumberPadAdd;
    case 0xE026: return VirtualKey::NumberPadSeparator;
    case 0xE027: return VirtualKey::NumberPadSubtract;
    case 0xE028: return VirtualKey::NumberPadDecimal;
    case 0xE029: return VirtualKey::NumberPadDivide;
    case 0xE031: return VirtualKey::Function1;
    case 0xE032: return VirtualKey::Function2;
    case 0xE033: return VirtualKey::Function3;
    case 0xE034: return VirtualKey::Function4;
    case 0xE035: return VirtualKey::Function5;
    case 0xE036: return VirtualKey::Function6;
    case 0xE037: return VirtualKey::Function7;
    case 0xE038: return VirtualKey::Function8;
    case 0xE039: return VirtualKey::Function9;
    case 0xE03A: return VirtualKey::Function10;
    case 0xE03B: return VirtualKey::Function11;
    case 0xE03C: return VirtualKey::Function12;
    case 0xE03D: return VirtualKey::Meta;
    case 0xE053: return VirtualKey::MetaRight;
    default: return std::nullopt;
    }
}

// https://w3c.github.io/webdriver/#dfn-dispatch-tick-actions
static std::expected<Vector<SimulatedInputKeyFrame>, AutomationCommandError> translateTicksToKeyFrames(State& state, const ActionsByTick& actionsByTick, Vector<Action>* cancelEntries)
{
    HashMap<String, SimulatedInputSourceState> projected;
    auto currentState = [&](const Action& action) -> SimulatedInputSourceState& {
        return projected.ensure(action.id, [&] {
            return state.simulatedSource(action).state;
        }).iterator->value;
    };

    Vector<SimulatedInputKeyFrame> keyFrames;
    for (auto& tick : actionsByTick) {
        std::optional<uint64_t> tickDuration;
        Vector<SimulatedInputKeyFrame::StateEntry> entries;
        for (auto& action : tick) {
            auto& sourceState = currentState(action);
            sourceState.duration = std::nullopt;
            switch (action.type) {
            case ActionType::Pause:
                break;
            case ActionType::KeyDown:
                if (auto virtualKey = virtualKeyForKeyValue(action.value))
                    sourceState.pressedVirtualKeys.add(normalizedVirtualKey(*virtualKey), *virtualKey);
                else {
#if ENABLE(WEBDRIVER_KEYBOARD_GRAPHEME_CLUSTERS)
                    sourceState.pressedCharKeys.add(action.value);
#else
                    auto charKey = pressedCharKey(action.value);
                    if (!charKey)
                        return makeUnexpected(AUTOMATION_COMMAND_ERROR_WITH_NAME_AND_MESSAGE(InvalidParameter, "Invalid key value."_s));
                    sourceState.pressedCharKeys.add(*charKey);
#endif
                }
                if (cancelEntries) {
                    auto keyUp = action;
                    keyUp.type = ActionType::KeyUp;
                    cancelEntries->append(WTF::move(keyUp));
                }
                break;
            case ActionType::KeyUp:
                if (auto virtualKey = virtualKeyForKeyValue(action.value))
                    sourceState.pressedVirtualKeys.remove(normalizedVirtualKey(*virtualKey));
                else {
#if ENABLE(WEBDRIVER_KEYBOARD_GRAPHEME_CLUSTERS)
                    sourceState.pressedCharKeys.remove(action.value);
#else
                    auto charKey = pressedCharKey(action.value);
                    if (!charKey)
                        return makeUnexpected(AUTOMATION_COMMAND_ERROR_WITH_NAME_AND_MESSAGE(InvalidParameter, "Invalid key value."_s));
                    sourceState.pressedCharKeys.remove(*charKey);
#endif
                }
                break;
            default:
                return makeUnexpected(AUTOMATION_COMMAND_ERROR_WITH_NAME_AND_MESSAGE(NotImplemented, "This input source type is not supported yet."_s));
            }
            if (action.duration)
                tickDuration = std::max(tickDuration.value_or(0), *action.duration);
            entries.append({ state.simulatedSource(action), sourceState });
        }
        if (tickDuration && !entries.isEmpty())
            entries.first().second.duration = Seconds::fromMilliseconds(*tickDuration);
        keyFrames.append(SimulatedInputKeyFrame { WTF::move(entries) });
    }

    return keyFrames;
}

static std::expected<Vector<SimulatedInputKeyFrame>, AutomationCommandError> keyFramesForActions(State& state, const ActionsByTick& actionsByTick)
{
    Vector<Action> newCancelEntries;
    auto keyFrames = translateTicksToKeyFrames(state, actionsByTick, &newCancelEntries);
    if (!keyFrames)
        return keyFrames;

    state.inputCancelList().appendVector(WTF::move(newCancelEntries));
    return keyFrames;
}

// https://w3c.github.io/webdriver/#dfn-dispatch-tick-actions
static std::expected<Vector<SimulatedInputKeyFrame>, AutomationCommandError> keyFramesForUndoActions(State& state, Vector<Action>&& undoActions)
{
    ActionsByTick ticks;
    ticks.reserveInitialCapacity(undoActions.size());
    for (auto& action : undoActions) {
        Vector<Action> tick;
        tick.append(WTF::move(action));
        ticks.append(WTF::move(tick));
    }

    return translateTicksToKeyFrames(state, ticks, nullptr);
}

#endif // ENABLE(WEBDRIVER_ACTIONS_API)

} // namespace WebKit::BidiInput

namespace WebKit {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(BidiInputAgent);

BidiInputAgent::BidiInputAgent(WebAutomationSession& session, BackendDispatcher& backendDispatcher)
    : m_session(session)
    , m_inputDomainDispatcher(BidiInputBackendDispatcher::create(backendDispatcher, this))
{
}

BidiInputAgent::~BidiInputAgent() = default;

BidiInput::State& BidiInputAgent::inputStateForTopLevelContext(const String& pageHandle)
{
    return m_inputStates.ensure(pageHandle, [] {
        return BidiInput::State::create();
    }).iterator->value.get();
}

void BidiInputAgent::willClosePage(const String& pageHandle)
{
    m_inputStates.remove(pageHandle);
}

void BidiInputAgent::performActions(const String& context, Ref<JSON::Array>&& actions, CommandCallback<void>&& callback)
{
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    // "Let navigable be the result of trying to get a navigable with navigable id."
    auto handles = session->extractBrowsingContextHandles(context);
    ASYNC_FAIL_IF_UNEXPECTED_RESULT(handles);
    auto& [pageHandle, frameHandle] = handles.value();

    Ref inputState = inputStateForTopLevelContext(pageHandle);
    auto actionsByTick = BidiInput::extractActionSequence(inputState, actions.get());
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!actionsByTick, InvalidParameter, actionsByTick.error());

#if ENABLE(WEBDRIVER_ACTIONS_API)
    // https://w3c.github.io/webdriver/#dfn-actions-queue
    inputState->enqueue([weakSession = WeakPtr { *session }, inputState, pageHandle, frameHandle, actionsByTick = WTF::move(*actionsByTick), callback = WTF::move(callback)](CompletionHandler<void()>&& done) mutable {
        RefPtr session = weakSession.get();
        if (!session) {
            callback(makeUnexpected(STRING_FOR_PREDEFINED_ERROR_NAME(InternalError)));
            return done();
        }

        // https://w3c.github.io/webdriver/#dfn-dispatch-actions
        auto keyFrames = BidiInput::keyFramesForActions(inputState, actionsByTick);
        if (!keyFrames) {
            callback(makeUnexpected(keyFrames.error().toProtocolString()));
            return done();
        }

        session->runBidiInputKeyFrames(pageHandle, frameHandle, WTF::move(*keyFrames), inputState->simulatedSources(), [inputState, callback = WTF::move(callback), done = WTF::move(done)](std::optional<AutomationCommandError> error) mutable {
            if (error)
                callback(makeUnexpected(error->toProtocolString()));
            else
                callback({ });
            done();
        });
    });
#else
    ASYNC_FAIL_WITH_PREDEFINED_ERROR(NotImplemented);
#endif
}

void BidiInputAgent::releaseActions(const String& context, CommandCallback<void>&& callback)
{
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    auto handles = session->extractBrowsingContextHandles(context);
    ASYNC_FAIL_IF_UNEXPECTED_RESULT(handles);
    auto& [pageHandle, frameHandle] = handles.value();

    Ref inputState = inputStateForTopLevelContext(pageHandle);

#if ENABLE(WEBDRIVER_ACTIONS_API)
    // https://w3c.github.io/webdriver/#dfn-actions-queue
    inputState->enqueue([weakSession = WeakPtr { *session }, inputState, pageHandle, frameHandle, callback = WTF::move(callback)](CompletionHandler<void()>&& done) mutable {
        RefPtr session = weakSession.get();
        if (!session) {
            callback(makeUnexpected(STRING_FOR_PREDEFINED_ERROR_NAME(InternalError)));
            return done();
        }

        // https://w3c.github.io/webdriver/#dfn-input-cancel-list
        auto undoActions = std::exchange(inputState->inputCancelList(), { });
        undoActions.reverse();
        auto keyFrames = BidiInput::keyFramesForUndoActions(inputState, WTF::move(undoActions));
        if (!keyFrames) {
            callback(makeUnexpected(keyFrames.error().toProtocolString()));
            return done();
        }

        session->runBidiInputKeyFrames(pageHandle, frameHandle, WTF::move(*keyFrames), inputState->simulatedSources(), [inputState, callback = WTF::move(callback), done = WTF::move(done)](std::optional<AutomationCommandError> error) mutable {
            // https://w3c.github.io/webdriver/#dfn-reset-the-input-state
            inputState->reset();
            if (error)
                callback(makeUnexpected(error->toProtocolString()));
            else
                callback({ });
            done();
        });
    });
#else
    inputState->reset();
    callback({ });
#endif
}

void BidiInputAgent::setFiles(const String&, Ref<JSON::Object>&&, Ref<JSON::Array>&&, CommandCallback<void>&& callback)
{
    // FIXME: Implement input.setFiles (https://webkit.org/b/287926).
    ASYNC_FAIL_WITH_PREDEFINED_ERROR(NotImplemented);
}

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)
