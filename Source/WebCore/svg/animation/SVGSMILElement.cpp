/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#include "CSSPropertyNames.h"
#include "Document.h"
#include "Event.h"
#include "EventListener.h"
#include "EventNames.h"
#include "EventSender.h"
#include "FloatConversion.h"
#include "FrameView.h"
#include "SMILTimeContainer.h"
#include "SVGDocumentExtensions.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include "SVGSVGElement.h"
#include "SVGURIReference.h"
#include "SVGUseElement.h"
#include "XLinkNames.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGSMILElement);

static SMILEventSender& smilBeginEventSender()
{
    static NeverDestroyed<SMILEventSender> sender(eventNames().beginEventEvent);
    return sender;
}

static SMILEventSender& smilEndEventSender()
{
    static NeverDestroyed<SMILEventSender> sender(eventNames().endEventEvent);
    return sender;
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

    SVGSMILElement* m_animation;
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
    if (!m_animation)
        return;
    m_animation->handleConditionEvent(m_condition);
}

SVGSMILElement::Condition::Condition(Type type, BeginOrEnd beginOrEnd, const String& baseID, const String& name, SMILTime offset, int repeats)
    : m_type(type)
    , m_beginOrEnd(beginOrEnd)
    , m_baseID(baseID)
    , m_name(name)
    , m_offset(offset)
    , m_repeats(repeats) 
{
}
    
SVGSMILElement::SVGSMILElement(const QualifiedName& tagName, Document& doc)
    : SVGElement(tagName, doc)
    , m_attributeName(anyQName())
    , m_targetElement(nullptr)
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
    smilBeginEventSender().cancelEvent(*this);
    smilEndEventSender().cancelEvent(*this);
    disconnectConditions();
    if (m_timeContainer && m_targetElement && hasValidAttributeName())
        m_timeContainer->unschedule(this, m_targetElement, m_attributeName);
}

void SVGSMILElement::clearResourceReferences()
{
    document().accessSVGExtensions().removeAllTargetReferencesForElement(*this);
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

    String id;
    RefPtr<Element> target;
    auto& href = getAttribute(XLinkNames::hrefAttr);
    if (href.isEmpty())
        target = parentElement();
    else {
        auto result = SVGURIReference::targetElementFromIRIString(href.string(), treeScope());
        target = WTFMove(result.element);
        id = WTFMove(result.identifier);
    }
    SVGElement* svgTarget = is<SVGElement>(target) ? downcast<SVGElement>(target.get()) : nullptr;

    if (svgTarget && !svgTarget->isConnected())
        svgTarget = nullptr;

    if (svgTarget != targetElement())
        setTargetElement(svgTarget);

    if (!svgTarget) {
        // Do not register as pending if we are already pending this resource.
        if (document().accessSVGExtensions().isPendingResource(*this, id))
            return;

        if (!id.isEmpty()) {
            document().accessSVGExtensions().addPendingResource(id, *this);
            ASSERT(hasPendingResources());
        }
    } else {
        // Register us with the target in the dependencies map. Any change of hrefElement
        // that leads to relayout/repainting now informs us, so we can react to it.
        document().accessSVGExtensions().addElementReferencingTarget(*this, *svgTarget);
    }
}

inline QualifiedName SVGSMILElement::constructAttributeName() const
{
    auto parseResult = Document::parseQualifiedName(attributeWithoutSynchronization(SVGNames::attributeNameAttr));
    if (parseResult.hasException())
        return anyQName();

    AtomicString prefix, localName;
    std::tie(prefix, localName) = parseResult.releaseReturnValue();

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
    clearAnimatedType(m_targetElement);

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

Node::InsertedIntoAncestorResult SVGSMILElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    SVGElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (!insertionType.connectedToDocument)
        return InsertedIntoAncestorResult::Done;

    // Verify we are not in <use> instance tree.
    ASSERT(!isInShadowTree() || !is<SVGUseElement>(shadowHost()));

    updateAttributeName();

    auto owner = makeRefPtr(ownerSVGElement());
    if (!owner)
        return InsertedIntoAncestorResult::Done;

    m_timeContainer = &owner->timeContainer();
    m_timeContainer->setDocumentOrderIndexesDirty();

    // "If no attribute is present, the default begin value (an offset-value of 0) must be evaluated."
    if (!hasAttributeWithoutSynchronization(SVGNames::beginAttr))
        m_beginTimes.append(SMILTimeWithOrigin());

    if (m_isWaitingForFirstInterval)
        resolveFirstInterval();

    if (m_timeContainer)
        m_timeContainer->notifyIntervalsChanged();

    return InsertedIntoAncestorResult::NeedsPostInsertionCallback;
}

void SVGSMILElement::didFinishInsertingNode()
{
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

bool SVGSMILElement::hasValidAttributeName()
{
    return attributeName() != anyQName();
}

SMILTime SVGSMILElement::parseOffsetValue(const String& data)
{
    bool ok;
    double result = 0;
    String parse = data.stripWhiteSpace();
    if (parse.endsWith('h'))
        result = parse.left(parse.length() - 1).toDouble(&ok) * 60 * 60;
    else if (parse.endsWith("min"))
        result = parse.left(parse.length() - 3).toDouble(&ok) * 60;
    else if (parse.endsWith("ms"))
        result = parse.left(parse.length() - 2).toDouble(&ok) / 1000;
    else if (parse.endsWith('s'))
        result = parse.left(parse.length() - 1).toDouble(&ok);
    else
        result = parse.toDouble(&ok);
    if (!ok || !SMILTime(result).isFinite())
        return SMILTime::unresolved();
    return result;
}
    
SMILTime SVGSMILElement::parseClockValue(const String& data)
{
    if (data.isNull())
        return SMILTime::unresolved();
    
    String parse = data.stripWhiteSpace();

    static NeverDestroyed<const AtomicString> indefiniteValue("indefinite", AtomicString::ConstructFromLiteral);
    if (parse == indefiniteValue)
        return SMILTime::indefinite();

    double result = 0;
    bool ok;
    size_t doublePointOne = parse.find(':');
    size_t doublePointTwo = parse.find(':', doublePointOne + 1);
    if (doublePointOne == 2 && doublePointTwo == 5 && parse.length() >= 8) { 
        result += parse.substring(0, 2).toUIntStrict(&ok) * 60 * 60;
        if (!ok)
            return SMILTime::unresolved();
        result += parse.substring(3, 2).toUIntStrict(&ok) * 60;
        if (!ok)
            return SMILTime::unresolved();
        result += parse.substring(6).toDouble(&ok);
    } else if (doublePointOne == 2 && doublePointTwo == notFound && parse.length() >= 5) { 
        result += parse.substring(0, 2).toUIntStrict(&ok) * 60;
        if (!ok)
            return SMILTime::unresolved();
        result += parse.substring(3).toDouble(&ok);
    } else
        return parseOffsetValue(parse);

    if (!ok || !SMILTime(result).isFinite())
        return SMILTime::unresolved();
    return result;
}
    
static void sortTimeList(Vector<SMILTimeWithOrigin>& timeList)
{
    std::sort(timeList.begin(), timeList.end());
}
    
bool SVGSMILElement::parseCondition(const String& value, BeginOrEnd beginOrEnd)
{
    String parseString = value.stripWhiteSpace();
    
    double sign = 1.;
    bool ok;
    size_t pos = parseString.find('+');
    if (pos == notFound) {
        pos = parseString.find('-');
        if (pos != notFound)
            sign = -1.;
    }
    String conditionString;
    SMILTime offset = 0;
    if (pos == notFound)
        conditionString = parseString;
    else {
        conditionString = parseString.left(pos).stripWhiteSpace();
        String offsetString = parseString.substring(pos + 1).stripWhiteSpace();
        offset = parseOffsetValue(offsetString);
        if (offset.isUnresolved())
            return false;
        offset = offset * sign;
    }
    if (conditionString.isEmpty())
        return false;
    pos = conditionString.find('.');
    
    String baseID;
    String nameString;
    if (pos == notFound)
        nameString = conditionString;
    else {
        baseID = conditionString.left(pos);
        nameString = conditionString.substring(pos + 1);
    }
    if (nameString.isEmpty())
        return false;

    Condition::Type type;
    int repeats = -1;
    if (nameString.startsWith("repeat(") && nameString.endsWith(')')) {
        // FIXME: For repeat events we just need to add the data carrying TimeEvent class and 
        // fire the events at appropiate times.
        repeats = nameString.substring(7, nameString.length() - 8).toUIntStrict(&ok);
        if (!ok)
            return false;
        nameString = "repeat";
        type = Condition::EventBase;
    } else if (nameString == "begin" || nameString == "end") {
        if (baseID.isEmpty())
            return false;
        type = Condition::Syncbase;
    } else if (nameString.startsWith("accesskey(")) {
        // FIXME: accesskey() support.
        type = Condition::AccessKey;
    } else
        type = Condition::EventBase;
    
    m_conditions.append(Condition(type, beginOrEnd, baseID, nameString, offset, repeats));

    if (type == Condition::EventBase && beginOrEnd == End)
        m_hasEndEventConditions = true;

    return true;
}

void SVGSMILElement::parseBeginOrEnd(const String& parseString, BeginOrEnd beginOrEnd)
{
    Vector<SMILTimeWithOrigin>& timeList = beginOrEnd == Begin ? m_beginTimes : m_endTimes;
    if (beginOrEnd == End)
        m_hasEndEventConditions = false;
    HashSet<double> existing;
    for (auto& time : timeList)
        existing.add(time.time().value());
    for (auto& string : parseString.split(';')) {
        SMILTime value = parseClockValue(string);
        if (value.isUnresolved())
            parseCondition(string, beginOrEnd);
        else if (!existing.contains(value.value()))
            timeList.append(SMILTimeWithOrigin(value, SMILTimeWithOrigin::ParserOrigin));
    }
    sortTimeList(timeList);
}

bool SVGSMILElement::isSupportedAttribute(const QualifiedName& attrName)
{
    static const auto supportedAttributes = makeNeverDestroyed(HashSet<QualifiedName> {
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
    });
    return supportedAttributes.get().contains<SVGAttributeHashTranslator>(attrName);
}

void SVGSMILElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::beginAttr) {
        if (!m_conditions.isEmpty()) {
            disconnectConditions();
            m_conditions.clear();
            parseBeginOrEnd(attributeWithoutSynchronization(SVGNames::endAttr), End);
        }
        parseBeginOrEnd(value.string(), Begin);
        if (isConnected())
            connectConditions();
    } else if (name == SVGNames::endAttr) {
        if (!m_conditions.isEmpty()) {
            disconnectConditions();
            m_conditions.clear();
            parseBeginOrEnd(attributeWithoutSynchronization(SVGNames::beginAttr), Begin);
        }
        parseBeginOrEnd(value.string(), End);
        if (isConnected())
            connectConditions();
    } else if (name == SVGNames::onendAttr)
        setAttributeEventListener(eventNames().endEventEvent, name, value);
    else if (name == SVGNames::onbeginAttr)
        setAttributeEventListener(eventNames().beginEventEvent, name, value);
    else
        SVGElement::parseAttribute(name, value);
}

void SVGSMILElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGElement::svgAttributeChanged(attrName);
        return;
    }

    if (attrName == SVGNames::durAttr)
        m_cachedDur = invalidCachedTime;
    else if (attrName == SVGNames::repeatDurAttr)
        m_cachedRepeatDur = invalidCachedTime;
    else if (attrName == SVGNames::repeatCountAttr)
        m_cachedRepeatCount = invalidCachedTime;
    else if (attrName == SVGNames::minAttr)
        m_cachedMin = invalidCachedTime;
    else if (attrName == SVGNames::maxAttr)
        m_cachedMax = invalidCachedTime;
    else if (attrName == SVGNames::attributeNameAttr)
        updateAttributeName();
    else if (attrName.matches(SVGNames::hrefAttr) || attrName.matches(XLinkNames::hrefAttr)) {
        InstanceInvalidationGuard guard(*this);
        buildPendingResource();
    } else if (isConnected()) {
        if (attrName == SVGNames::beginAttr)
            beginListChanged(elapsed());
        else if (attrName == SVGNames::endAttr)
            endListChanged(elapsed());
    }

    animationAttributeChanged();
}

inline Element* SVGSMILElement::eventBaseFor(const Condition& condition)
{
    return condition.m_baseID.isEmpty() ? targetElement() : treeScope().getElementById(condition.m_baseID);
}

void SVGSMILElement::connectConditions()
{
    if (m_conditionsConnected)
        disconnectConditions();
    m_conditionsConnected = true;
    for (auto& condition : m_conditions) {
        if (condition.m_type == Condition::EventBase) {
            ASSERT(!condition.m_syncbase);
            auto eventBase = makeRefPtr(eventBaseFor(condition));
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
            if (!is<SVGSMILElement>(*condition.m_syncbase)) {
                condition.m_syncbase = nullptr;
                continue;
            }
            downcast<SVGSMILElement>(*condition.m_syncbase).addTimeDependent(this);
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
            auto eventBase = makeRefPtr(eventBaseFor(condition));
            if (eventBase)
                eventBase->removeEventListener(condition.m_name, *condition.m_eventListener, false);
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
    if (m_timeContainer && m_targetElement && m_attributeName != attributeName) {
        if (hasValidAttributeName())
            m_timeContainer->unschedule(this, m_targetElement, m_attributeName);
        m_attributeName = attributeName;
        if (hasValidAttributeName())
            m_timeContainer->schedule(this, m_targetElement, m_attributeName);
    } else
        m_attributeName = attributeName;

    // Only clear the animated type, if we had a target before.
    if (m_targetElement)
        clearAnimatedType(m_targetElement);
}

void SVGSMILElement::setTargetElement(SVGElement* target)
{
    if (m_timeContainer && hasValidAttributeName()) {
        if (m_targetElement)
            m_timeContainer->unschedule(this, m_targetElement, m_attributeName);
        if (target)
            m_timeContainer->schedule(this, target, m_attributeName);
    }

    if (m_targetElement) {
        // Clear values that may depend on the previous target.
        clearAnimatedType(m_targetElement);
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

bool SVGSMILElement::isInactive() const
{
     return m_activeState == Inactive;
}

bool SVGSMILElement::isFrozen() const
{
    return m_activeState == Frozen;
}
    
SVGSMILElement::Restart SVGSMILElement::restart() const
{    
    static NeverDestroyed<const AtomicString> never("never", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<const AtomicString> whenNotActive("whenNotActive", AtomicString::ConstructFromLiteral);
    const AtomicString& value = attributeWithoutSynchronization(SVGNames::restartAttr);
    if (value == never)
        return RestartNever;
    if (value == whenNotActive)
        return RestartWhenNotActive;
    return RestartAlways;
}
    
SVGSMILElement::FillMode SVGSMILElement::fill() const
{   
    static NeverDestroyed<const AtomicString> freeze("freeze", AtomicString::ConstructFromLiteral);
    const AtomicString& value = attributeWithoutSynchronization(SVGNames::fillAttr);
    return value == freeze ? FillFreeze : FillRemove;
}
    
SMILTime SVGSMILElement::dur() const
{   
    if (m_cachedDur != invalidCachedTime)
        return m_cachedDur;
    const AtomicString& value = attributeWithoutSynchronization(SVGNames::durAttr);
    SMILTime clockValue = parseClockValue(value);
    return m_cachedDur = clockValue <= 0 ? SMILTime::unresolved() : clockValue;
}

SMILTime SVGSMILElement::repeatDur() const
{    
    if (m_cachedRepeatDur != invalidCachedTime)
        return m_cachedRepeatDur;
    const AtomicString& value = attributeWithoutSynchronization(SVGNames::repeatDurAttr);
    SMILTime clockValue = parseClockValue(value);
    m_cachedRepeatDur = clockValue <= 0 ? SMILTime::unresolved() : clockValue;
    return m_cachedRepeatDur;
}
    
// So a count is not really a time but let just all pretend we did not notice.
SMILTime SVGSMILElement::repeatCount() const
{    
    if (m_cachedRepeatCount != invalidCachedTime)
        return m_cachedRepeatCount;
    const AtomicString& value = attributeWithoutSynchronization(SVGNames::repeatCountAttr);
    if (value.isNull())
        return SMILTime::unresolved();

    static NeverDestroyed<const AtomicString> indefiniteValue("indefinite", AtomicString::ConstructFromLiteral);
    if (value == indefiniteValue)
        return SMILTime::indefinite();
    bool ok;
    double result = value.string().toDouble(&ok);
    return m_cachedRepeatCount = ok && result > 0 ? result : SMILTime::unresolved();
}

SMILTime SVGSMILElement::maxValue() const
{    
    if (m_cachedMax != invalidCachedTime)
        return m_cachedMax;
    const AtomicString& value = attributeWithoutSynchronization(SVGNames::maxAttr);
    SMILTime result = parseClockValue(value);
    return m_cachedMax = (result.isUnresolved() || result <= 0) ? SMILTime::indefinite() : result;
}
    
SMILTime SVGSMILElement::minValue() const
{    
    if (m_cachedMin != invalidCachedTime)
        return m_cachedMin;
    const AtomicString& value = attributeWithoutSynchronization(SVGNames::minAttr);
    SMILTime result = parseClockValue(value);
    return m_cachedMin = (result.isUnresolved() || result < 0) ? 0 : result;
}
                 
SMILTime SVGSMILElement::simpleDuration() const
{
    return std::min(dur(), SMILTime::indefinite());
}

void SVGSMILElement::addBeginTime(SMILTime eventTime, SMILTime beginTime, SMILTimeWithOrigin::Origin origin)
{
    ASSERT(!std::isnan(beginTime.value()));
    m_beginTimes.append(SMILTimeWithOrigin(beginTime, origin));
    sortTimeList(m_beginTimes);
    beginListChanged(eventTime);
}

void SVGSMILElement::addEndTime(SMILTime eventTime, SMILTime endTime, SMILTimeWithOrigin::Origin origin)
{
    ASSERT(!std::isnan(endTime.value()));
    m_endTimes.append(SMILTimeWithOrigin(endTime, origin));
    sortTimeList(m_endTimes);
    endListChanged(eventTime);
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
        return beginOrEnd == Begin ? SMILTime::unresolved() : SMILTime::indefinite();
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
        bool wasUnresolved = m_intervalBegin.isUnresolved();
        m_intervalBegin = begin;
        m_intervalEnd = end;
        notifyDependentsIntervalChanged(wasUnresolved ? NewInterval : ExistingInterval);
        m_nextProgressTime = std::min(m_nextProgressTime, m_intervalBegin);

        if (m_timeContainer)
            m_timeContainer->notifyIntervalsChanged();
    }
}

void SVGSMILElement::resolveNextInterval(bool notifyDependents)
{
    SMILTime begin;
    SMILTime end;
    resolveInterval(false, begin, end);
    ASSERT(!begin.isIndefinite());
    
    if (!begin.isUnresolved() && begin != m_intervalBegin) {
        m_intervalBegin = begin;
        m_intervalEnd = end;
        if (notifyDependents)
            notifyDependentsIntervalChanged(NewInterval);
        m_nextProgressTime = std::min(m_nextProgressTime, m_intervalBegin);
    }
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
                notifyDependentsIntervalChanged(ExistingInterval);
            }
        }
    }
    m_nextProgressTime = elapsed();

    if (m_timeContainer)
        m_timeContainer->notifyIntervalsChanged();
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
                notifyDependentsIntervalChanged(ExistingInterval);
            }
        }
    }
    m_nextProgressTime = elapsed;

    if (m_timeContainer)
        m_timeContainer->notifyIntervalsChanged();
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
            notifyDependentsIntervalChanged(ExistingInterval);
        }
    }

    if (elapsed >= m_intervalEnd)
        resolveNextInterval(true);
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
            resolveNextInterval(false);
            continue;
        }

        // If the desired 'elapsed' time is past the current interval, advance to the next.
        if (elapsed >= m_intervalEnd) {
            resolveNextInterval(false);
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
        repeat = static_cast<unsigned>(repeatingDuration.value() / simpleDuration.value()) - 1;

        double percent = (m_intervalEnd.value() - m_intervalBegin.value()) / simpleDuration.value();
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
    ASSERT(m_timeContainer);
    if (m_activeState == Active) {
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
    
bool SVGSMILElement::progress(SMILTime elapsed, SVGSMILElement* resultElement, bool seekToTime)
{
    ASSERT(resultElement);
    ASSERT(m_timeContainer);
    ASSERT(m_isWaitingForFirstInterval || m_intervalBegin.isFinite());

    if (!m_intervalBegin.isFinite()) {
        ASSERT(m_activeState == Inactive);
        m_nextProgressTime = SMILTime::unresolved();
        return false;
    }

    if (elapsed < m_intervalBegin) {
        ASSERT(m_activeState != Active);
        if (m_activeState == Frozen) {
            if (this == resultElement)
                resetAnimatedType();
            updateAnimation(m_lastPercent, m_lastRepeat, resultElement);
        }
        m_nextProgressTime = m_intervalBegin;
        return false;
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

    // Only reset the animated type to the base value once for the lowest priority animation that animates and contributes to a particular element/attribute pair.
    if (this == resultElement && animationIsContributing)
        resetAnimatedType();

    if (animationIsContributing) {
        if (oldActiveState == Inactive)
            startedActiveInterval();

        updateAnimation(percent, repeat, resultElement);
        m_lastPercent = percent;
        m_lastRepeat = repeat;
    }

    if (oldActiveState == Active && m_activeState != Active) {
        smilEndEventSender().dispatchEventSoon(*this);
        endedActiveInterval();
        if (m_activeState != Frozen)
            clearAnimatedType(m_targetElement);
    } else if (oldActiveState != Active && m_activeState == Active)
        smilBeginEventSender().dispatchEventSoon(*this);

    // Triggering all the pending events if the animation timeline is changed.
    if (seekToTime) {
        if (m_activeState == Inactive || m_activeState == Frozen)
            smilEndEventSender().dispatchEventSoon(*this);
    }

    m_nextProgressTime = calculateNextProgressTime(elapsed);
    return animationIsContributing;
}
    
void SVGSMILElement::notifyDependentsIntervalChanged(NewOrExistingInterval newOrExisting)
{
    ASSERT(m_intervalBegin.isFinite());
    static NeverDestroyed<HashSet<SVGSMILElement*>> loopBreaker;
    if (loopBreaker.get().contains(this))
        return;
    loopBreaker.get().add(this);
    
    for (auto& dependent : m_timeDependents) {
        dependent->createInstanceTimesFromSyncbase(this, newOrExisting);
    }

    loopBreaker.get().remove(this);
}
    
void SVGSMILElement::createInstanceTimesFromSyncbase(SVGSMILElement* syncbase, NewOrExistingInterval)
{
    // FIXME: To be really correct, this should handle updating exising interval by changing 
    // the associated times instead of creating new ones.
    for (auto& condition : m_conditions) {
        if (condition.m_type == Condition::Syncbase && condition.m_syncbase == syncbase) {
            ASSERT(condition.m_name == "begin" || condition.m_name == "end");
            // No nested time containers in SVG, no need for crazy time space conversions. Phew!
            SMILTime time = 0;
            if (condition.m_name == "begin")
                time = syncbase->m_intervalBegin + condition.m_offset;
            else
                time = syncbase->m_intervalEnd + condition.m_offset;
            if (!time.isFinite())
                continue;
            if (condition.m_beginOrEnd == Begin)
                addBeginTime(elapsed(), time);
            else
                addEndTime(elapsed(), time);
        }
    }
}
    
void SVGSMILElement::addTimeDependent(SVGSMILElement* animation)
{
    m_timeDependents.add(animation);
    if (m_intervalBegin.isFinite())
        animation->createInstanceTimesFromSyncbase(this, NewInterval);
}
    
void SVGSMILElement::removeTimeDependent(SVGSMILElement* animation)
{
    m_timeDependents.remove(animation);
}
    
void SVGSMILElement::handleConditionEvent(Condition* condition)
{
    SMILTime elapsed = this->elapsed();
    if (condition->m_beginOrEnd == Begin)
        addBeginTime(elapsed, elapsed + condition->m_offset);
    else
        addEndTime(elapsed, elapsed + condition->m_offset);
}

void SVGSMILElement::beginByLinkActivation()
{
    SMILTime elapsed = this->elapsed();
    addBeginTime(elapsed, elapsed);
}

void SVGSMILElement::endedActiveInterval()
{
    clearTimesWithDynamicOrigins(m_beginTimes);
    clearTimesWithDynamicOrigins(m_endTimes);
}

void SVGSMILElement::dispatchPendingEvent(SMILEventSender* eventSender)
{
    ASSERT(eventSender == &smilBeginEventSender() || eventSender == &smilEndEventSender());
    const AtomicString& eventType = eventSender->eventType();
    dispatchEvent(Event::create(eventType, Event::CanBubble::No, Event::IsCancelable::No));
}

}
