/*
 * Copyright (C) 2008-2024 Apple Inc. All rights reserved.
 * Copyright (C) 2013-2021 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "SVGSMILElement.h"

#include "AddEventListenerOptions.h"
#include "CSSPropertyNames.h"
#include "Document.h"
#include "Event.h"
#include "EventListener.h"
#include "EventNames.h"
#include "EventSender.h"
#include "FloatConversion.h"
#include "LocalFrameView.h"
#include "NodeName.h"
#include "Page.h"
#include "SMILTimeContainer.h"
#include "SVGDocumentExtensions.h"
#include "SVGElementInlines.h"
#include "SVGElementTypeHelpers.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include "SVGSVGElement.h"
#include "SVGURIReference.h"
#include "SVGUseElement.h"
#include "SVGVisitedElementTracking.h"
#include "XLinkNames.h"
#include <wtf/MathExtras.h>
#include <wtf/RobinHoodHashSet.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Vector.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(SVGSMILElement);

static SMILEventSender& smilEventSender()
{
    static NeverDestroyed<SMILEventSender> sender;
    return sender;
}

static const AtomString& indefiniteAtom()
{
    static MainThreadNeverDestroyed<const AtomString> indefiniteValue("indefinite"_s);
    return indefiniteValue;
}

// This is used for duration type time values that can't be negative.
static const double invalidCachedTime = -1.;
    
class ConditionEventListener final : public EventListener {
public:
    static Ref<ConditionEventListener> create(SVGSMILElement* animation, SVGSMILElement::Condition* condition)
    {
        return adoptRef(*new ConditionEventListener(animation, condition));
    }

    static const ConditionEventListener* cast(const EventListener* listener)
    {
        return listener->type() == ConditionEventListenerType
            ? static_cast<const ConditionEventListener*>(listener)
            : nullptr;
    }

    bool operator==(const EventListener& other) const final;
    
    void disconnectAnimation()
    {
        m_animation = nullptr;
    }

private:
    ConditionEventListener(SVGSMILElement* animation, SVGSMILElement::Condition* condition) 
        : EventListener(ConditionEventListenerType)
        , m_animation(animation)
        , m_condition(condition) 
    {
    }

    void handleEvent(ScriptExecutionContext&, Event&) final;

    WeakPtr<SVGSMILElement, WeakPtrImplWithEventTargetData> m_animation;
    SVGSMILElement::Condition* m_condition;
};

bool ConditionEventListener::operator==(const EventListener& listener) const
{
    if (const ConditionEventListener* conditionEventListener = ConditionEventListener::cast(&listener))
        return m_animation == conditionEventListener->m_animation && m_condition == conditionEventListener->m_condition;
    return false;
}

void ConditionEventListener::handleEvent(ScriptExecutionContext&, Event&)
{
    if (RefPtr animation = m_animation.get())
        animation->addInstanceTime(m_condition->m_beginOrEnd, m_animation->elapsed() + m_condition->m_offset);
}

SVGSMILElement::Condition::Condition(Type type, BeginOrEnd beginOrEnd, const String& baseID, const AtomString& name, SMILTime offset, int repeats)
    : m_type(type)
    , m_beginOrEnd(beginOrEnd)
    , m_baseID(baseID)
    , m_name(name)
    , m_offset(offset)
    , m_repeats(repeats) 
{
}
    
SVGSMILElement::SVGSMILElement(const QualifiedName& tagName, Document& doc, UniqueRef<SVGPropertyRegistry>&& propertyRegistry)
    : SVGElement(tagName, doc, WTFMove(propertyRegistry))
    , m_attributeName(anyQName())
    , m_conditionsConnected(false)
    , m_hasEndEventConditions(false)
    , m_isWaitingForFirstInterval(true)
    , m_intervalBegin(SMILTime::unresolved())
    , m_intervalEnd(SMILTime::unresolved())
    , m_previousIntervalBegin(SMILTime::unresolved())
    , m_activeState(Inactive)
    , m_lastPercent(0)
    , m_lastRepeat(0)
    , m_nextProgressTime(0)
    , m_documentOrderIndex(0)
    , m_cachedDur(invalidCachedTime)
    , m_cachedRepeatDur(invalidCachedTime)
    , m_cachedRepeatCount(invalidCachedTime)
    , m_cachedMin(invalidCachedTime)
    , m_cachedMax(invalidCachedTime)
{
    resolveFirstInterval();
}

SVGSMILElement::~SVGSMILElement()
{
    clearResourceReferences();
    smilEventSender().cancelEvent(*this);
    disconnectConditions();
    if (RefPtr timeContainer = m_timeContainer; timeContainer && m_targetElement && hasValidAttributeName())
        timeContainer->unschedule(this, protectedTargetElement().get(), m_attributeName);
}

void SVGSMILElement::clearResourceReferences()
{
    removeElementReference();
}

void SVGSMILElement::clearTarget()
{
    setTargetElement(nullptr);
}

void SVGSMILElement::buildPendingResource()
{
    clearResourceReferences();

    if (!isConnected()) {
        // Reset the target element if we are no longer in the document.
        setTargetElement(nullptr);
        return;
    }

    AtomString id;
    RefPtr<Element> target;
    auto& href = getAttribute(SVGNames::hrefAttr, XLinkNames::hrefAttr);
    if (href.isEmpty())
        target = parentElement();
    else {
        auto result = SVGURIReference::targetElementFromIRIString(href.string(), treeScopeForSVGReferences());
        target = WTFMove(result.element);
        id = WTFMove(result.identifier);
    }
    RefPtr svgTarget = target && target->isConnected() ? dynamicDowncast<SVGElement>(*target) : nullptr;

    if (svgTarget != targetElement())
        setTargetElement(svgTarget.get());

    if (!svgTarget) {
        // Do not register as pending if we are already pending this resource.
        if (treeScopeForSVGReferences().isPendingSVGResource(*this, id))
            return;

        if (!id.isEmpty()) {
            treeScopeForSVGReferences().addPendingSVGResource(id, *this);
            ASSERT(hasPendingResources());
        }
    } else
        svgTarget->addReferencingElement(*this);
}

bool SVGSMILElement::hasPresentationalHintsForAttribute(const QualifiedName& name) const
{
    // https://svgwg.org/svg2-draft/styling.html#PresentationAttributes
    if (name == SVGNames::fillAttr)
        return false;
    return StyledElement::hasPresentationalHintsForAttribute(name);
}

inline QualifiedName SVGSMILElement::constructAttributeName() const
{
    auto parseResult = Document::parseQualifiedName(attributeWithoutSynchronization(SVGNames::attributeNameAttr));
    if (parseResult.hasException())
        return anyQName();

    auto [prefix, localName] = parseResult.releaseReturnValue();

    if (prefix.isNull())
        return { nullAtom(), localName, nullAtom() };

    auto namespaceURI = lookupNamespaceURI(prefix);
    if (namespaceURI.isEmpty())
        return anyQName();

    return { nullAtom(), localName, namespaceURI };
}

inline void SVGSMILElement::updateAttributeName()
{
    setAttributeName(constructAttributeName());
}

static inline void clearTimesWithDynamicOrigins(Vector<SMILTimeWithOrigin>& timeList)
{
    timeList.removeAllMatching([] (const SMILTimeWithOrigin& time) {
        return time.originIsScript();
    });
}

void SVGSMILElement::reset()
{
    stopAnimation(protectedTargetElement().get());

    m_activeState = Inactive;
    m_isWaitingForFirstInterval = true;
    m_intervalBegin = SMILTime::unresolved();
    m_intervalEnd = SMILTime::unresolved();
    m_previousIntervalBegin = SMILTime::unresolved();
    m_lastPercent = 0;
    m_lastRepeat = 0;
    m_nextProgressTime = 0;
    resolveFirstInterval();
}

RefPtr<SMILTimeContainer> SVGSMILElement::protectedTimeContainer() const
{
    return m_timeContainer;
}

Node::InsertedIntoAncestorResult SVGSMILElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    SVGElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (!insertionType.connectedToDocument)
        return InsertedIntoAncestorResult::Done;

    // Verify we are not in <use> instance tree.
    ASSERT(!isInShadowTree() || !is<SVGUseElement>(shadowHost()));

    updateAttributeName();

    RefPtr owner = ownerSVGElement();
    if (!owner)
        return InsertedIntoAncestorResult::Done;

    m_timeContainer = &owner->timeContainer();
    protectedTimeContainer()->setDocumentOrderIndexesDirty();

    // "If no attribute is present, the default begin value (an offset-value of 0) must be evaluated."
    if (!hasAttributeWithoutSynchronization(SVGNames::beginAttr))
        m_beginTimes.append(SMILTimeWithOrigin());

    if (m_isWaitingForFirstInterval)
        resolveFirstInterval();

    if (RefPtr timeContainer = m_timeContainer)
        timeContainer->notifyIntervalsChanged();

    return InsertedIntoAncestorResult::NeedsPostInsertionCallback;
}

void SVGSMILElement::didFinishInsertingNode()
{
    SVGElement::didFinishInsertingNode();
    buildPendingResource();
}

void SVGSMILElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    if (removalType.disconnectedFromDocument) {
        clearResourceReferences();
        disconnectConditions();
        setTargetElement(nullptr);
        setAttributeName(anyQName());
        animationAttributeChanged();
        m_timeContainer = nullptr;
    }

    SVGElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
}

bool SVGSMILElement::hasValidAttributeName() const
{
    return attributeName() != anyQName();
}

SMILTime SVGSMILElement::parseOffsetValue(StringView data)
{
    bool ok;
    double result = 0;
    auto parse = data.trim(isUnicodeCompatibleASCIIWhitespace<UChar>);
    if (parse.endsWith('h'))
        result = parse.left(parse.length() - 1).toDouble(ok) * 60 * 60;
    else if (parse.endsWith("min"_s))
        result = parse.left(parse.length() - 3).toDouble(ok) * 60;
    else if (parse.endsWith("ms"_s))
        result = parse.left(parse.length() - 2).toDouble(ok) / 1000;
    else if (parse.endsWith('s'))
        result = parse.left(parse.length() - 1).toDouble(ok);
    else
        result = parse.toDouble(ok);
    if (!ok || !SMILTime(result).isFinite())
        return SMILTime::unresolved();
    return result;
}
    
SMILTime SVGSMILElement::parseClockValue(StringView data)
{
    if (data.isNull())
        return SMILTime::unresolved();

    auto parse = data.trim(isUnicodeCompatibleASCIIWhitespace<UChar>);
    if (parse == indefiniteAtom())
        return SMILTime::indefinite();

    double result = 0;
    bool ok;
    size_t doublePointOne = parse.find(':');
    size_t doublePointTwo = parse.find(':', doublePointOne + 1);
    if (doublePointOne == 2 && doublePointTwo == 5 && parse.length() >= 8) {
        auto hour = parseInteger<uint8_t>(parse.left(2));
        auto minute = parseInteger<uint8_t>(parse.substring(3, 2));
        if (!hour || !minute)
            return SMILTime::unresolved();
        result = *hour * 60 * 60 + *minute * 60 + parse.substring(6).toDouble(ok);
    } else if (doublePointOne == 2 && doublePointTwo == notFound && parse.length() >= 5) { 
        auto minute = parseInteger<uint8_t>(parse.left(2));
        if (!minute)
            return SMILTime::unresolved();
        result = *minute * 60 + parse.substring(3).toDouble(ok);
    } else
        return parseOffsetValue(parse);
    if (!ok || !SMILTime(result).isFinite())
        return SMILTime::unresolved();
    return result;
}
    
bool SVGSMILElement::parseCondition(StringView value, BeginOrEnd beginOrEnd)
{
    auto parseString = value.trim(isUnicodeCompatibleASCIIWhitespace<UChar>);
    
    double sign = 1.;
    size_t pos = parseString.find('+');
    if (pos == notFound) {
        pos = parseString.find('-');
        if (pos != notFound)
            sign = -1.;
    }
    StringView conditionString;
    SMILTime offset = 0;
    if (pos == notFound)
        conditionString = parseString;
    else {
        conditionString = parseString.left(pos).trim(isUnicodeCompatibleASCIIWhitespace<UChar>);
        auto offsetString = parseString.substring(pos + 1).trim(isUnicodeCompatibleASCIIWhitespace<UChar>);
        offset = parseOffsetValue(offsetString);
        if (offset.isUnresolved())
            return false;
        offset = offset * sign;
    }
    if (conditionString.isEmpty())
        return false;
    pos = conditionString.find('.');
    
    StringView baseID;
    StringView nameView;
    if (pos == notFound)
        nameView = conditionString;
    else {
        baseID = conditionString.left(pos);
        nameView = conditionString.substring(pos + 1);
    }
    if (nameView.isEmpty())
        return false;

    AtomString nameString;
    Condition::Type type;
    int repeats = -1;
    if (nameView.startsWith("repeat("_s) && nameView.endsWith(')')) {
        // FIXME: For repeat events we just need to add the data carrying TimeEvent class and fire the events at appropiate times.
        auto parsedRepeat = parseInteger<unsigned>(nameView.substring(7, nameView.length() - 8));
        if (!parsedRepeat)
            return false;
        // FIXME: By assigning an unsigned to a signed, this can turn large integers into negative numbers.
        repeats = *parsedRepeat;
        nameString = "repeat"_s;
        type = Condition::EventBase;
    } else if (nameView == "begin"_s || nameView == "end"_s) {
        if (baseID.isEmpty())
            return false;
        type = Condition::Syncbase;
        nameString = nameView.toAtomString();
    } else if (nameView.startsWith("accesskey("_s)) {
        // FIXME: accesskey() support.
        type = Condition::AccessKey;
        nameString = nameView.toAtomString();
    } else {
        type = Condition::EventBase;
        nameString = nameView.toAtomString();
    }
    
    m_conditions.append(Condition(type, beginOrEnd, baseID.toString(), WTFMove(nameString), offset, repeats));

    if (type == Condition::EventBase && beginOrEnd == End)
        m_hasEndEventConditions = true;

    return true;
}

void SVGSMILElement::parseBeginOrEnd(StringView parseString, BeginOrEnd beginOrEnd)
{
    Vector<SMILTimeWithOrigin>& timeList = beginOrEnd == Begin ? m_beginTimes : m_endTimes;
    if (beginOrEnd == End)
        m_hasEndEventConditions = false;
    HashSet<double> existing;
    for (auto& time : timeList)
        existing.add(time.time().value());
    for (auto string : parseString.split(';')) {
        SMILTime value = parseClockValue(string);
        if (value.isUnresolved())
            parseCondition(string, beginOrEnd);
        else if (!existing.contains(value.value()))
            timeList.append(SMILTimeWithOrigin(value, SMILTimeWithOrigin::ParserOrigin));
    }
    std::sort(timeList.begin(), timeList.end());
}

bool SVGSMILElement::isSupportedAttribute(const QualifiedName& attrName)
{
    static NeverDestroyed supportedAttributes = MemoryCompactLookupOnlyRobinHoodHashSet<QualifiedName> {
        SVGNames::beginAttr,
        SVGNames::endAttr,
        SVGNames::durAttr,
        SVGNames::repeatDurAttr,
        SVGNames::repeatCountAttr,
        SVGNames::minAttr,
        SVGNames::maxAttr,
        SVGNames::attributeNameAttr,
        SVGNames::hrefAttr,
        XLinkNames::hrefAttr,
    };
    return supportedAttributes.get().contains<SVGAttributeHashTranslator>(attrName);
}

void SVGSMILElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    SVGElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);

    switch (name.nodeName()) {
    case AttributeNames::beginAttr:
        if (!m_conditions.isEmpty()) {
            disconnectConditions();
            m_conditions.clear();
            parseBeginOrEnd(attributeWithoutSynchronization(SVGNames::endAttr), End);
        }
        parseBeginOrEnd(newValue, Begin);
        if (isConnected())
            connectConditions();
        break;
    case AttributeNames::endAttr:
        if (!m_conditions.isEmpty()) {
            disconnectConditions();
            m_conditions.clear();
            parseBeginOrEnd(attributeWithoutSynchronization(SVGNames::beginAttr), Begin);
        }
        parseBeginOrEnd(newValue, End);
        if (isConnected())
            connectConditions();
        break;
    case AttributeNames::onendAttr:
        setAttributeEventListener(eventNames().endEventEvent, name, newValue);
        break;
    case AttributeNames::onbeginAttr:
        setAttributeEventListener(eventNames().beginEventEvent, name, newValue);
        break;
    default:
        break;
    }
}

void SVGSMILElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGElement::svgAttributeChanged(attrName);
        return;
    }

    switch (attrName.nodeName()) {
    case AttributeNames::durAttr:
        m_cachedDur = invalidCachedTime;
        break;
    case AttributeNames::repeatDurAttr:
        m_cachedRepeatDur = invalidCachedTime;
        break;
    case AttributeNames::repeatCountAttr:
        m_cachedRepeatCount = invalidCachedTime;
        break;
    case AttributeNames::minAttr:
        m_cachedMin = invalidCachedTime;
        break;
    case AttributeNames::maxAttr:
        m_cachedMax = invalidCachedTime;
        break;
    case AttributeNames::attributeNameAttr:
        updateAttributeName();
        break;
    case AttributeNames::hrefAttr:
    case AttributeNames::XLink::hrefAttr: {
        InstanceInvalidationGuard guard(*this);
        buildPendingResource();
        break;
    }
    case AttributeNames::beginAttr:
        if (isConnected())
            beginListChanged(elapsed());
        break;
    case AttributeNames::endAttr:
        if (isConnected())
            endListChanged(elapsed());
        break;
    default:
        break;
    }
    animationAttributeChanged();
}

inline RefPtr<Element> SVGSMILElement::eventBaseFor(const Condition& condition)
{
    return condition.m_baseID.isEmpty() ? RefPtr<Element> { targetElement() } : treeScope().getElementById(condition.m_baseID);
}

void SVGSMILElement::connectConditions()
{
    if (m_conditionsConnected)
        disconnectConditions();
    m_conditionsConnected = true;
    for (auto& condition : m_conditions) {
        if (condition.m_type == Condition::EventBase) {
            ASSERT(!condition.m_syncbase);
            RefPtr eventBase = eventBaseFor(condition);
            if (!eventBase)
                continue;
            ASSERT(!condition.m_eventListener);
            condition.m_eventListener = ConditionEventListener::create(this, &condition);
            eventBase->addEventListener(condition.m_name, *condition.m_eventListener, false);
        } else if (condition.m_type == Condition::Syncbase) {
            ASSERT(!condition.m_baseID.isEmpty());
            condition.m_syncbase = treeScope().getElementById(condition.m_baseID);
            if (!condition.m_syncbase)
                continue;
            RefPtr svgSMILElement = dynamicDowncast<SVGSMILElement>(*condition.m_syncbase);
            if (!svgSMILElement) {
                condition.m_syncbase = nullptr;
                continue;
            }
            svgSMILElement->addTimeDependent(this);
        }
    }
}

void SVGSMILElement::disconnectConditions()
{
    if (!m_conditionsConnected)
        return;
    m_conditionsConnected = false;
    for (auto& condition : m_conditions) {
        if (condition.m_type == Condition::EventBase) {
            ASSERT(!condition.m_syncbase);
            if (!condition.m_eventListener)
                continue;
            // Note: It's a memory optimization to try to remove our condition
            // event listener, but it's not guaranteed to work, since we have
            // no guarantee that eventBaseFor() will be able to find our condition's
            // original eventBase. So, we also have to disconnect ourselves from
            // our condition event listener, in case it later fires.
            RefPtr eventBase = eventBaseFor(condition);
            if (eventBase)
                eventBase->removeEventListener(condition.m_name, Ref { *condition.m_eventListener }, false);
            condition.m_eventListener->disconnectAnimation();
            condition.m_eventListener = nullptr;
        } else if (condition.m_type == Condition::Syncbase) {
            if (condition.m_syncbase)
                downcast<SVGSMILElement>(condition.m_syncbase.get())->removeTimeDependent(this);
        }
        condition.m_syncbase = nullptr;
    }
}

void SVGSMILElement::setAttributeName(const QualifiedName& attributeName)
{
    if (RefPtr timeContainer = m_timeContainer; timeContainer && m_targetElement && m_attributeName != attributeName) {
        if (hasValidAttributeName())
            timeContainer->unschedule(this, protectedTargetElement().get(), m_attributeName);
        m_attributeName = attributeName;
        if (hasValidAttributeName())
            timeContainer->schedule(this, protectedTargetElement().get(), m_attributeName);
    } else
        m_attributeName = attributeName;

    // Only clear the animated type, if we had a target before.
    if (RefPtr targetElement = m_targetElement.get())
        stopAnimation(targetElement.get());
}

void SVGSMILElement::setTargetElement(SVGElement* target)
{
    if (RefPtr timeContainer = m_timeContainer; timeContainer && hasValidAttributeName()) {
        if (RefPtr targetElement = m_targetElement.get())
            timeContainer->unschedule(this, targetElement.get(), m_attributeName);
        if (target)
            timeContainer->schedule(this, target, m_attributeName);
    }

    if (RefPtr targetElement = m_targetElement.get()) {
        // Clear values that may depend on the previous target.
        stopAnimation(targetElement.get());
        disconnectConditions();
    }

    // If the animation state is not Inactive, always reset to a clear state before leaving the old target element.
    if (m_activeState != Inactive)
        endedActiveInterval();

    m_targetElement = target;
}

SMILTime SVGSMILElement::elapsed() const
{
    return m_timeContainer ? m_timeContainer->elapsed() : 0;
}

bool SVGSMILElement::isFrozen() const
{
    return m_activeState == Frozen;
}
    
SVGSMILElement::Restart SVGSMILElement::restart() const
{    
    static MainThreadNeverDestroyed<const AtomString> never("never"_s);
    static MainThreadNeverDestroyed<const AtomString> whenNotActive("whenNotActive"_s);
    const AtomString& value = attributeWithoutSynchronization(SVGNames::restartAttr);
    if (value == never)
        return RestartNever;
    if (value == whenNotActive)
        return RestartWhenNotActive;
    return RestartAlways;
}
    
SVGSMILElement::FillMode SVGSMILElement::fill() const
{   
    static MainThreadNeverDestroyed<const AtomString> freeze("freeze"_s);
    const AtomString& value = attributeWithoutSynchronization(SVGNames::fillAttr);
    return value == freeze ? FillFreeze : FillRemove;
}
    
SMILTime SVGSMILElement::dur() const
{   
    if (m_cachedDur != invalidCachedTime)
        return m_cachedDur;
    const AtomString& value = attributeWithoutSynchronization(SVGNames::durAttr);
    SMILTime clockValue = parseClockValue(value);
    return m_cachedDur = clockValue <= 0 ? SMILTime::unresolved() : clockValue;
}

SMILTime SVGSMILElement::repeatDur() const
{    
    if (m_cachedRepeatDur != invalidCachedTime)
        return m_cachedRepeatDur;
    const AtomString& value = attributeWithoutSynchronization(SVGNames::repeatDurAttr);
    SMILTime clockValue = parseClockValue(value);
    m_cachedRepeatDur = clockValue <= 0 ? SMILTime::unresolved() : clockValue;
    return m_cachedRepeatDur;
}
    
// So a count is not really a time but let just all pretend we did not notice.
SMILTime SVGSMILElement::repeatCount() const
{    
    if (m_cachedRepeatCount != invalidCachedTime)
        return m_cachedRepeatCount;
    const AtomString& value = attributeWithoutSynchronization(SVGNames::repeatCountAttr);
    if (value.isNull())
        return SMILTime::unresolved();

    if (value == indefiniteAtom())
        return SMILTime::indefinite();
    bool ok;
    double result = value.string().toDouble(&ok);
    return m_cachedRepeatCount = (ok && result > 0 && std::isfinite(result)) ? result : SMILTime::unresolved();
}

SMILTime SVGSMILElement::maxValue() const
{    
    if (m_cachedMax != invalidCachedTime)
        return m_cachedMax;
    const AtomString& value = attributeWithoutSynchronization(SVGNames::maxAttr);
    SMILTime result = parseClockValue(value);
    return m_cachedMax = (result.isUnresolved() || result <= 0) ? SMILTime::indefinite() : result;
}
    
SMILTime SVGSMILElement::minValue() const
{    
    if (m_cachedMin != invalidCachedTime)
        return m_cachedMin;
    const AtomString& value = attributeWithoutSynchronization(SVGNames::minAttr);
    SMILTime result = parseClockValue(value);
    return m_cachedMin = (result.isUnresolved() || result < 0) ? 0 : result;
}
                 
SMILTime SVGSMILElement::simpleDuration() const
{
    return std::min(dur(), SMILTime::indefinite());
}

static void insertSorted(Vector<SMILTimeWithOrigin>& list, SMILTimeWithOrigin time)
{
    ASSERT(std::is_sorted(list.begin(), list.end()));
    list.insert(std::lower_bound(list.begin(), list.end(), time) - list.begin(), time);
}

void SVGSMILElement::addInstanceTime(BeginOrEnd beginOrEnd, SMILTime time, SMILTimeWithOrigin::Origin origin)
{
    SMILTime elapsed = this->elapsed();
    if (elapsed.isUnresolved())
        return;
    insertSorted(beginOrEnd == Begin ? m_beginTimes : m_endTimes, SMILTimeWithOrigin(time, origin));
    if (beginOrEnd == Begin)
        beginListChanged(elapsed);
    else
        endListChanged(elapsed);
}

inline SMILTime extractTimeFromVector(const SMILTimeWithOrigin* position)
{
    return position->time();
}

SMILTime SVGSMILElement::findInstanceTime(BeginOrEnd beginOrEnd, SMILTime minimumTime, bool equalsMinimumOK) const
{
    const Vector<SMILTimeWithOrigin>& list = beginOrEnd == Begin ? m_beginTimes : m_endTimes;
    int sizeOfList = list.size();

    if (!sizeOfList)
        return beginOrEnd == Begin ? SMILTime::unresolved() : SMILTime::indefinite();

    const SMILTimeWithOrigin* result = approximateBinarySearch<const SMILTimeWithOrigin, SMILTime>(list, sizeOfList, minimumTime, extractTimeFromVector);
    int indexOfResult = result - list.begin();
    ASSERT_WITH_SECURITY_IMPLICATION(indexOfResult < sizeOfList);

    if (list[indexOfResult].time() < minimumTime && indexOfResult < sizeOfList - 1)
        ++indexOfResult;

    const SMILTime& currentTime = list[indexOfResult].time();

    // The special value "indefinite" does not yield an instance time in the begin list.
    if (currentTime.isIndefinite() && beginOrEnd == Begin)
        return SMILTime::unresolved();

    if (currentTime < minimumTime)
        return SMILTime::unresolved();
    if (currentTime > minimumTime)
        return currentTime;

    ASSERT(currentTime == minimumTime);
    if (equalsMinimumOK)
        return currentTime;

    // If the equals is not accepted, return the next bigger item in the list.
    SMILTime nextTime = currentTime;
    while (indexOfResult < sizeOfList - 1) {
        nextTime = list[indexOfResult + 1].time();
        if (nextTime > minimumTime)
            return nextTime;
        ++indexOfResult;
    }

    return beginOrEnd == Begin ? SMILTime::unresolved() : SMILTime::indefinite();
}

SMILTime SVGSMILElement::repeatingDuration() const
{
    // Computing the active duration
    // http://www.w3.org/TR/SMIL2/smil-timing.html#Timing-ComputingActiveDur
    SMILTime repeatCount = this->repeatCount();
    SMILTime repeatDur = this->repeatDur();
    SMILTime simpleDuration = this->simpleDuration();
    if (!simpleDuration || (repeatDur.isUnresolved() && repeatCount.isUnresolved()))
        return simpleDuration;
    SMILTime repeatCountDuration = simpleDuration * repeatCount;
    return std::min(repeatCountDuration, std::min(repeatDur, SMILTime::indefinite()));
}

SMILTime SVGSMILElement::resolveActiveEnd(SMILTime resolvedBegin, SMILTime resolvedEnd) const
{
    // Computing the active duration
    // http://www.w3.org/TR/SMIL2/smil-timing.html#Timing-ComputingActiveDur
    SMILTime preliminaryActiveDuration;
    if (!resolvedEnd.isUnresolved() && dur().isUnresolved() && repeatDur().isUnresolved() && repeatCount().isUnresolved())
        preliminaryActiveDuration = resolvedEnd - resolvedBegin;
    else if (!resolvedEnd.isFinite())
        preliminaryActiveDuration = repeatingDuration();
    else
        preliminaryActiveDuration = std::min(repeatingDuration(), resolvedEnd - resolvedBegin);
    
    SMILTime minValue = this->minValue();
    SMILTime maxValue = this->maxValue();
    if (minValue > maxValue) {
        // Ignore both.
        // http://www.w3.org/TR/2001/REC-smil-animation-20010904/#MinMax
        minValue = 0;
        maxValue = SMILTime::indefinite();
    }
    return resolvedBegin + std::min(maxValue, std::max(minValue, preliminaryActiveDuration));
}

void SVGSMILElement::resolveInterval(bool first, SMILTime& beginResult, SMILTime& endResult) const
{
    // See the pseudocode in http://www.w3.org/TR/SMIL3/smil-timing.html#q90.
    SMILTime beginAfter = first ? -std::numeric_limits<double>::infinity() : m_intervalEnd;
    SMILTime lastIntervalTempEnd = std::numeric_limits<double>::infinity();
    while (true) {
        bool equalsMinimumOK = !first || m_intervalEnd > m_intervalBegin;
        SMILTime tempBegin = findInstanceTime(Begin, beginAfter, equalsMinimumOK);
        if (tempBegin.isUnresolved())
            break;
        SMILTime tempEnd;
        if (m_endTimes.isEmpty())
            tempEnd = resolveActiveEnd(tempBegin, SMILTime::indefinite());
        else {
            tempEnd = findInstanceTime(End, tempBegin, true);
            if ((first && tempBegin == tempEnd && tempEnd == lastIntervalTempEnd) || (!first && tempEnd == m_intervalEnd)) 
                tempEnd = findInstanceTime(End, tempBegin, false);    
            if (tempEnd.isUnresolved()) {
                if (!m_endTimes.isEmpty() && !m_hasEndEventConditions)
                    break;
            }
            tempEnd = resolveActiveEnd(tempBegin, tempEnd);
        }
        if (!first || (tempEnd > 0 || (!tempBegin.value() && !tempEnd.value()))) {
            beginResult = tempBegin;
            endResult = tempEnd;
            return;
        }

        beginAfter = tempEnd;
        lastIntervalTempEnd = tempEnd;
    }
    beginResult = SMILTime::unresolved();
    endResult = SMILTime::unresolved();
}
    
void SVGSMILElement::resolveFirstInterval()
{
    SMILTime begin;
    SMILTime end;
    resolveInterval(true, begin, end);
    ASSERT(!begin.isIndefinite());

    if (!begin.isUnresolved() && (begin != m_intervalBegin || end != m_intervalEnd)) {   
        m_intervalBegin = begin;
        m_intervalEnd = end;
        notifyDependentsIntervalChanged();
        m_nextProgressTime = std::min(m_nextProgressTime, m_intervalBegin);

        if (RefPtr timeContainer = m_timeContainer)
            timeContainer->notifyIntervalsChanged();
    }
}

bool SVGSMILElement::resolveNextInterval()
{
    SMILTime begin;
    SMILTime end;
    resolveInterval(false, begin, end);
    ASSERT(!begin.isIndefinite());
    
    if (!begin.isUnresolved() && begin != m_intervalBegin) {
        m_intervalBegin = begin;
        m_intervalEnd = end;
        notifyDependentsIntervalChanged();
        m_nextProgressTime = std::min(m_nextProgressTime, m_intervalBegin);
        return true;
    }
    return false;
}

SMILTime SVGSMILElement::nextProgressTime() const
{
    return m_nextProgressTime;
}
    
void SVGSMILElement::beginListChanged(SMILTime eventTime)
{
    if (m_isWaitingForFirstInterval)
        resolveFirstInterval();
    else {
        if (restart() == RestartNever)
            return;

        SMILTime newBegin = findInstanceTime(Begin, eventTime, true);
        if (newBegin.isFinite() && (m_intervalEnd <= eventTime || newBegin < m_intervalBegin)) {
            // Begin time changed, re-resolve the interval.
            SMILTime oldBegin = m_intervalBegin;
            m_intervalEnd = eventTime;
            resolveInterval(false, m_intervalBegin, m_intervalEnd);  
            ASSERT(!m_intervalBegin.isUnresolved());
            if (m_intervalBegin != oldBegin) {
                if (m_activeState == Active && m_intervalBegin > eventTime) {
                    m_activeState = determineActiveState(eventTime);
                    if (m_activeState != Active)
                        endedActiveInterval();
                }
                notifyDependentsIntervalChanged();
            }
        }
    }
    m_nextProgressTime = elapsed();

    if (RefPtr timeContainer = m_timeContainer)
        timeContainer->notifyIntervalsChanged();
}

void SVGSMILElement::endListChanged(SMILTime)
{
    SMILTime elapsed = this->elapsed();
    if (m_isWaitingForFirstInterval)
        resolveFirstInterval();
    else if (elapsed < m_intervalEnd && m_intervalBegin.isFinite()) {
        SMILTime newEnd = findInstanceTime(End, m_intervalBegin, false);
        if (newEnd < m_intervalEnd) {
            newEnd = resolveActiveEnd(m_intervalBegin, newEnd);
            if (newEnd != m_intervalEnd) {
                m_intervalEnd = newEnd;
                notifyDependentsIntervalChanged();
            }
        }
    }
    m_nextProgressTime = elapsed;

    if (RefPtr timeContainer = m_timeContainer)
        timeContainer->notifyIntervalsChanged();
}

void SVGSMILElement::checkRestart(SMILTime elapsed)
{
    ASSERT(!m_isWaitingForFirstInterval);
    ASSERT(elapsed >= m_intervalBegin);

    Restart restart = this->restart();
    if (restart == RestartNever)
        return;

    if (elapsed < m_intervalEnd) {
        if (restart != RestartAlways)
            return;
        SMILTime nextBegin = findInstanceTime(Begin, m_intervalBegin, false);
        if (nextBegin < m_intervalEnd) { 
            m_intervalEnd = nextBegin;
            notifyDependentsIntervalChanged();
        }
    }

    if (elapsed >= m_intervalEnd)
        resolveNextInterval();
}

void SVGSMILElement::seekToIntervalCorrespondingToTime(SMILTime elapsed)
{
    ASSERT(!m_isWaitingForFirstInterval);
    ASSERT(elapsed >= m_intervalBegin);

    // Manually seek from interval to interval, just as if the animation would run regulary.
    while (true) {
        // Figure out the next value in the begin time list after the current interval begin.
        SMILTime nextBegin = findInstanceTime(Begin, m_intervalBegin, false);

        // If the 'nextBegin' time is unresolved (eg. just one defined interval), we're done seeking.
        if (nextBegin.isUnresolved())
            return;

        // If the 'nextBegin' time is larger than or equal to the current interval end time, we're done seeking.
        // If the 'elapsed' time is smaller than the next begin interval time, we're done seeking.
        if (nextBegin < m_intervalEnd && elapsed >= nextBegin) {
            // End current interval, and start a new interval from the 'nextBegin' time.
            m_intervalEnd = nextBegin;
            if (!resolveNextInterval())
                break;
            continue;
        }

        // If the desired 'elapsed' time is past the current interval, advance to the next.
        if (elapsed >= m_intervalEnd) {
            if (!resolveNextInterval())
                break;
            continue;
        }

        return;
    }
}

float SVGSMILElement::calculateAnimationPercentAndRepeat(SMILTime elapsed, unsigned& repeat) const
{
    SMILTime simpleDuration = this->simpleDuration();
    repeat = 0;
    if (simpleDuration.isIndefinite()) {
        repeat = 0;
        return 0.f;
    }
    if (!simpleDuration) {
        repeat = 0;
        return 1.f;
    }
    ASSERT(m_intervalBegin.isFinite());
    ASSERT(simpleDuration.isFinite());
    SMILTime activeTime = elapsed - m_intervalBegin;
    SMILTime repeatingDuration = this->repeatingDuration();
    if (elapsed >= m_intervalEnd || activeTime > repeatingDuration) {
        repeat = static_cast<unsigned>(repeatingDuration.value() / simpleDuration.value());
        if (!fmod(repeatingDuration.value(), simpleDuration.value()))
            --repeat;

        double lastActiveDuration = elapsed >= m_intervalEnd ? m_intervalEnd.value() - m_intervalBegin.value() : repeatingDuration.value();
        double percent = lastActiveDuration / simpleDuration.value();
        percent = percent - floor(percent);
        if (percent < std::numeric_limits<float>::epsilon() || 1 - percent < std::numeric_limits<float>::epsilon())
            return 1.0f;
        return narrowPrecisionToFloat(percent);
    }
    repeat = static_cast<unsigned>(activeTime.value() / simpleDuration.value());
    SMILTime simpleTime = fmod(activeTime.value(), simpleDuration.value());
    return narrowPrecisionToFloat(simpleTime.value() / simpleDuration.value());
}
    
SMILTime SVGSMILElement::calculateNextProgressTime(SMILTime elapsed) const
{
    if (m_timeContainer && m_activeState == Active) {
        // If duration is indefinite the value does not actually change over time. Same is true for <set>.
        SMILTime simpleDuration = this->simpleDuration();
        if (simpleDuration.isIndefinite() || hasTagName(SVGNames::setTag)) {
            SMILTime repeatingDurationEnd = m_intervalBegin + repeatingDuration();
            // We are supposed to do freeze semantics when repeating ends, even if the element is still active. 
            // Take care that we get a timer callback at that point.
            if (elapsed < repeatingDurationEnd && repeatingDurationEnd < m_intervalEnd && repeatingDurationEnd.isFinite())
                return repeatingDurationEnd;
            return m_intervalEnd;
        } 
        return elapsed + m_timeContainer->animationFrameDelay();
    }
    return m_intervalBegin >= elapsed ? m_intervalBegin : SMILTime::unresolved();
}
    
SVGSMILElement::ActiveState SVGSMILElement::determineActiveState(SMILTime elapsed) const
{
    if (elapsed >= m_intervalBegin && elapsed < m_intervalEnd)
        return Active;

    return fill() == FillFreeze ? Frozen : Inactive;
}
    
bool SVGSMILElement::isContributing(SMILTime elapsed) const 
{ 
    // Animation does not contribute during the active time if it is past its repeating duration and has fill=remove.
    return (m_activeState == Active && (fill() == FillFreeze || elapsed <= m_intervalBegin + repeatingDuration())) || m_activeState == Frozen;
}
    
bool SVGSMILElement::progress(SMILTime elapsed, SVGSMILElement& firstAnimation, bool seekToTime)
{
    ASSERT(m_timeContainer);
    ASSERT(m_isWaitingForFirstInterval || m_intervalBegin.isFinite());

    if (!m_conditionsConnected)
        connectConditions();

    if (!m_intervalBegin.isFinite()) {
        ASSERT(m_activeState == Inactive);
        m_nextProgressTime = SMILTime::unresolved();
        return false;
    }

    if (elapsed < m_intervalBegin) {
        ASSERT(m_activeState != Active);
        bool isFrozen = (m_activeState == Frozen);
        if (isFrozen) {
            if (this == &firstAnimation)
                startAnimation();
            updateAnimation(m_lastPercent, m_lastRepeat);
        }
        m_nextProgressTime = m_intervalBegin;
        // If the animation is frozen, it's still contributing.
        return isFrozen;
    }

    m_previousIntervalBegin = m_intervalBegin;

    if (m_isWaitingForFirstInterval) {
        m_isWaitingForFirstInterval = false;
        resolveFirstInterval();
    }

    // This call may obtain a new interval -- never call calculateAnimationPercentAndRepeat() before!
    if (seekToTime) {
        seekToIntervalCorrespondingToTime(elapsed);
        if (elapsed < m_intervalBegin) {
            // elapsed is not within an interval.
            m_nextProgressTime = m_intervalBegin;
            return false;
        }
    }

    unsigned repeat = 0;
    float percent = calculateAnimationPercentAndRepeat(elapsed, repeat);
    checkRestart(elapsed);

    ActiveState oldActiveState = m_activeState;
    m_activeState = determineActiveState(elapsed);
    bool animationIsContributing = isContributing(elapsed);

    if (animationIsContributing) {
        // Only start the animation of the lowest priority animation that animates and contributes to a particular element/attribute pair.
        if (this == &firstAnimation)
            startAnimation();

        if (oldActiveState == Inactive)
            startedActiveInterval();

        updateAnimation(percent, repeat);
        m_lastPercent = percent;
        m_lastRepeat = repeat;
    }

    if (oldActiveState == Active && m_activeState != Active) {
        smilEventSender().dispatchEventSoon(*this, eventNames().endEventEvent);
        endedActiveInterval();
        if (m_activeState != Frozen)
            stopAnimation(protectedTargetElement().get());
    } else if (oldActiveState != Active && m_activeState == Active)
        smilEventSender().dispatchEventSoon(*this, eventNames().beginEventEvent);

    // Triggering all the pending events if the animation timeline is changed.
    if (seekToTime) {
        if (m_activeState == Inactive || m_activeState == Frozen)
            smilEventSender().dispatchEventSoon(*this, eventNames().endEventEvent);
    }

    m_nextProgressTime = calculateNextProgressTime(elapsed);
    return animationIsContributing;
}
    
void SVGSMILElement::notifyDependentsIntervalChanged()
{
    ASSERT(m_intervalBegin.isFinite());

    static NeverDestroyed<SVGVisitedElementTracking::VisitedSet> s_visitedSet;

    SVGVisitedElementTracking recursionTracking(s_visitedSet);
    if (recursionTracking.isVisiting(*this))
        return;

    SVGVisitedElementTracking::Scope recursionScope(recursionTracking, *this);

    for (Ref dependent : m_timeDependents)
        dependent->createInstanceTimesFromSyncbase(this);
}
    
void SVGSMILElement::createInstanceTimesFromSyncbase(SVGSMILElement* syncbase)
{
    // FIXME: To be really correct, this should handle updating exising interval by changing 
    // the associated times instead of creating new ones.
    for (auto& condition : m_conditions) {
        if (condition.m_type == Condition::Syncbase && condition.m_syncbase == syncbase) {
            ASSERT(condition.m_name == "begin"_s || condition.m_name == "end"_s);
            // No nested time containers in SVG, no need for crazy time space conversions. Phew!
            SMILTime time = 0;
            if (condition.m_name == "begin"_s)
                time = syncbase->m_intervalBegin + condition.m_offset;
            else
                time = syncbase->m_intervalEnd + condition.m_offset;
            if (!time.isFinite())
                continue;
            addInstanceTime(condition.m_beginOrEnd, time);
        }
    }
}
    
void SVGSMILElement::addTimeDependent(SVGSMILElement* animation)
{
    m_timeDependents.add(*animation);
    if (m_intervalBegin.isFinite())
        animation->createInstanceTimesFromSyncbase(this);
}
    
void SVGSMILElement::removeTimeDependent(SVGSMILElement* animation)
{
    m_timeDependents.remove(*animation);
}

void SVGSMILElement::beginByLinkActivation()
{
    addInstanceTime(Begin, elapsed());
}

void SVGSMILElement::endedActiveInterval()
{
    clearTimesWithDynamicOrigins(m_beginTimes);
    clearTimesWithDynamicOrigins(m_endTimes);
}

void SVGSMILElement::dispatchPendingEvent(SMILEventSender* eventSender, const AtomString& eventType)
{
    ASSERT_UNUSED(eventSender, eventSender == &smilEventSender());
    dispatchEvent(Event::create(eventType, Event::CanBubble::No, Event::IsCancelable::No));
}

}
