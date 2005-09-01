/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include <float.h>

#include <kdom/kdom.h>
#include <kdom/core/AttrImpl.h>
#include <kdom/core/CDFInterface.h>
#include <kdom/core/DOMImplementationImpl.h>
#include <kdom/css/CSSStyleDeclarationImpl.h>
#include <kdom/DOMString.h>

#include "svgattrs.h"
#include "SVGHelper.h"
#include "SVGDocumentImpl.h"
#include "SVGURIReferenceImpl.h"
#include "SVGStyledElementImpl.h"
#include "SVGAnimationElementImpl.h"

#include <cmath>

using namespace KSVG;
using namespace std;

SVGAnimationElementImpl::SVGAnimationElementImpl(KDOM::DocumentPtr *doc, KDOM::NodeImpl::Id id, KDOM::DOMStringImpl *prefix)
: SVGElementImpl(doc, id, prefix), SVGTestsImpl(), SVGExternalResourcesRequiredImpl()
{
    m_connected = false;

    m_targetElement = 0;
    
    m_currentTime = 0.0;
    m_simpleDuration = 0.0;

    // Initialize shared properties...
    m_end = 0.0;
    m_min = 0.0;
    m_max = 0.0;
    m_begin = 0.0;

    m_repeations = 0;
    m_repeatCount = 0;

    m_values = 0;
    m_keyTimes = 0;
    m_keySplines = 0;

    m_fill = FILL_REMOVE;
    m_restart = RESTART_ALWAYS;
    m_calcMode = CALCMODE_LINEAR;
    m_additive = ADDITIVE_REPLACE;
    m_accumulate = ACCUMULATE_NONE;
    m_attributeType = ATTRIBUTETYPE_AUTO;
}

SVGAnimationElementImpl::~SVGAnimationElementImpl()
{
    if(m_values)
        m_values->deref();
    if(m_keyTimes)
        m_keyTimes->deref();
    if(m_keySplines)
        m_keySplines->deref();
}

SVGElementImpl *SVGAnimationElementImpl::targetElement() const
{
    if(!m_targetElement)
    {
        if(!m_href.isEmpty())
        {
            KDOM::ElementImpl *element = ownerDocument()->getElementById(KDOM::DOMString(SVGURIReferenceImpl::getTarget(m_href)).handle());
            if(element)
                m_targetElement = dynamic_cast<SVGElementImpl *>(element);
        }
        else if(parentNode())
        {
            KDOM::NodeImpl *target = parentNode();
            while(target != 0)
            {
                if(target->nodeType() != KDOM::ELEMENT_NODE)
                    target = target->parentNode();
                else
                    break;
            }
            
            m_targetElement = static_cast<SVGElementImpl *>(target);
        }
    }
                        
    return m_targetElement;
}

double SVGAnimationElementImpl::getEndTime() const
{
    return m_end;
}

double SVGAnimationElementImpl::getStartTime() const
{
    return m_begin;
}

double SVGAnimationElementImpl::getCurrentTime() const
{
    return m_currentTime;
}

double SVGAnimationElementImpl::getSimpleDuration() const
{
    return m_simpleDuration;
}

void SVGAnimationElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
    int id = (attr->id() & NodeImpl_IdLocalMask);
    KDOM::DOMString value(attr->value());
    switch(id)
    {
        case ATTR_HREF:
        {
            m_href = value.string();
            break;
        }
        case ATTR_ATTRIBUTENAME:
        {
            m_attributeName = value.string();
            break;
        }
        case ATTR_ATTRIBUTETYPE:
        {
            if(value == "CSS")
                m_attributeType = ATTRIBUTETYPE_CSS;
            else if(value == "XML")
                m_attributeType = ATTRIBUTETYPE_XML;
            else if(value == "auto")
                m_attributeType = ATTRIBUTETYPE_AUTO;

            break;
        }                
        case ATTR_BEGIN:
        case ATTR_END:
        {
            // Create list
            SVGStringListImpl *temp = new SVGStringListImpl();
            temp->ref();

            // Feed data into list
            SVGHelper::ParseSeperatedList(temp, value.string(), ';');

            // Parse data
            for(unsigned int i = 0; i < temp->numberOfItems(); i++)
            {
                QString current = KDOM::DOMString(temp->getItem(i)).string();

                if(current.startsWith(QString::fromLatin1("accessKey")))
                {
                    // Register keyDownEventListener for the character
                    QString character = current.mid(current.length() - 2, 1);

                    kdDebug() << k_funcinfo << " Supposed to register accessKey Character: " << character << " UNSUPPORTED!" << endl;
                }
                else if(current.startsWith(QString::fromLatin1("wallclock")))
                {
                    int firstBrace = current.find('(');
                    int secondBrace = current.find(')');

                    QString wallclockValue = current.mid(firstBrace + 1, secondBrace - firstBrace - 2);
                    kdDebug() << k_funcinfo << " Supposed to use wallClock value: " << wallclockValue << " UNSUPPORTED!" << endl;
                }
                else if(current.contains('.'))
                {
                    int dotPosition = current.find('.');

                    QString element = current.mid(0, dotPosition);
                    QString clockValue;
                    if(current.contains(QString::fromLatin1("begin")))
                        clockValue = current.mid(dotPosition + 6);
                    else if(current.contains(QString::fromLatin1("end")))
                        clockValue = current.mid(dotPosition + 4);
                    else if(current.contains(QString::fromLatin1("repeat")))
                        clockValue = current.mid(dotPosition + 7);
                    else // DOM2 Event Reference
                    {
                        int plusMinusPosition = -1;

                        if(current.contains('+'))
                            plusMinusPosition = current.find('+');
                        else if(current.contains('-'))
                            plusMinusPosition = current.find('-');

                        QString event = current.mid(dotPosition + 1, plusMinusPosition - dotPosition - 1);
                        clockValue = current.mid(dotPosition + event.length() + 1);

                        kdDebug() << k_funcinfo << " Supposed to use DOM Event: " << event << " UNSUPPORTED!" << endl;
                    }
                }
                else
                {
                    if(id == ATTR_BEGIN)
                    {
                        m_begin = parseClockValue(current);
                        if(!isIndefinite(m_begin))
                            m_begin *= 1000.0;

                        kdDebug() << k_funcinfo << " Setting begin time to " << m_begin << " ms!" << endl;
                    }
                    else
                    {
                        m_end = parseClockValue(current);
                        if(!isIndefinite(m_end))
                            m_end *= 1000.0;

                        kdDebug() << k_funcinfo << " Setting end time to " << m_end << " ms!" << endl;
                    }
                }
            }

            temp->deref();
            break;
        }
        case ATTR_DUR:
        {
            m_simpleDuration = parseClockValue(value.string());
            if(!isIndefinite(m_simpleDuration))
                m_simpleDuration *= 1000.0;

            break;
        }
        case ATTR_MIN:
        {
            m_min = parseClockValue(value.string());
            if(!isIndefinite(m_min))
                m_min *= 1000.0;

            break;
        }
        case ATTR_MAX:
        {
            m_max = parseClockValue(value.string());
            if(!isIndefinite(m_max))
                m_max *= 1000.0;

            break;
        }
        case ATTR_RESTART:
        {
            if(value == "whenNotActive")
                m_restart = RESTART_WHENNOTACTIVE;
            else if(value == "never")
                m_restart = RESTART_NEVER;
            else if(value == "always")
                m_restart = RESTART_ALWAYS;

            break;
        }
        case ATTR_REPEATCOUNT:
        {
            if(value == "indefinite")
                m_repeatCount = DBL_MAX;
            else
                m_repeatCount = value.string().toDouble();
                
            break;
        }
        case ATTR_REPEATDUR:
        {
            m_repeatDur = value.string();
            break;
        }
        case ATTR_FILL:
        {
            if(value == "freeze")
                m_fill = FILL_FREEZE;
            else if(value == "remove")
                m_fill = FILL_REMOVE;

            break;
        }
        case ATTR_CALCMODE:
        {
            if(value == "discrete")
                m_calcMode = CALCMODE_DISCRETE;
            else if(value == "linear")
                m_calcMode = CALCMODE_LINEAR;
            else if(value == "spline")
                m_calcMode = CALCMODE_SPLINE;
            else if(value == "paced")
                m_calcMode = CALCMODE_PACED;
            
            break;
        }
        case ATTR_VALUES:
        {
            if(m_values)
                m_values->deref();

            m_values = new SVGStringListImpl();
            m_values->ref();
                        
            SVGHelper::ParseSeperatedList(m_values, value.string(), ';');
            break;
        }
        case ATTR_KEYTIMES:
        {
            if(m_keyTimes)
                m_keyTimes->deref();

            m_keyTimes = new SVGStringListImpl();
            m_keyTimes->ref();
                        
            SVGHelper::ParseSeperatedList(m_keyTimes, value.string(), ';');
            break;
        }
        case ATTR_KEYSPLINES:
        {
            if(m_keySplines)
                m_keySplines->deref();

            m_keySplines = new SVGStringListImpl();
            m_keySplines->ref();
                        
            SVGHelper::ParseSeperatedList(m_keySplines, value.string(), ';');
            break;
        }
        case ATTR_FROM:
        {
            m_from = value.string();
            break;
        }
        case ATTR_TO:
        {
            m_to = value.string();
            break;
        }
        case ATTR_BY:
        {
            m_by = value.string();
            break;
        }
        case ATTR_ADDITIVE:
        {
            if(value == "sum")
                m_additive = ADDITIVE_SUM;
            else if(value == "replace")
                m_additive = ADDITIVE_REPLACE;
                        
            break;
        }
        case ATTR_ACCUMULATE:
        {
            if(value == "sum")
                m_accumulate = ACCUMULATE_SUM;
            else if(value == "none")
                m_accumulate = ACCUMULATE_NONE;
                        
            break;
        }
        default:
        {
            if(SVGTestsImpl::parseAttribute(attr)) return;
            if(SVGExternalResourcesRequiredImpl::parseAttribute(attr)) return;
            
            SVGElementImpl::parseAttribute(attr);
        }
    };
}

double SVGAnimationElementImpl::parseClockValue(const QString &data) const
{
    QString parse = data.stripWhiteSpace();
    
    if(parse == QString::fromLatin1("indefinite")) // Saves some time...
        return DBL_MAX;

    double result;

    int doublePointOne = parse.find(':');
    int doublePointTwo = parse.find(':', doublePointOne + 1);

    if(doublePointOne != -1 && doublePointTwo != -1) // Spec: "Full clock values"
    {
        unsigned int hours = parse.mid(0, 2).toUInt();
        unsigned int minutes = parse.mid(3, 2).toUInt();
        unsigned int seconds = parse.mid(6, 2).toUInt();
        unsigned int milliseconds = 0;

        result = (3600 * hours) + (60 * minutes) + seconds;

        if(parse.find('.') != -1)
        {
            QString temp = parse.mid(9, 2);
            milliseconds = temp.toUInt();
            result += (milliseconds * (1 / pow(10.0, int(temp.length()))));
        }
    }
    else if(doublePointOne != -1 && doublePointTwo == -1) // Spec: "Partial clock values"
    {
        unsigned int minutes = parse.mid(0, 2).toUInt();
        unsigned int seconds = parse.mid(3, 2).toUInt();
        unsigned int milliseconds = 0;

        result = (60 * minutes) + seconds;

        if(parse.find('.') != -1)
        {
            QString temp = parse.mid(6, 2);
            milliseconds = temp.toUInt();
            result += (milliseconds * (1 / pow(10.0, int(temp.length()))));
        }
    }
    else // Spec: "Timecount values"
    {
        int dotPosition = parse.find('.');

        if(parse.endsWith(QString::fromLatin1("h")))
        {
            if(dotPosition == -1)
                result = parse.mid(0, parse.length() - 1).toUInt() * 3600;
            else
            {
                result = parse.mid(0, dotPosition).toUInt() * 3600;
                QString temp = parse.mid(dotPosition + 1, parse.length() - dotPosition - 2);
                result += (3600.0 * temp.toUInt()) * (1 / pow(10.0, int(temp.length())));
            }
        }
        else if(parse.endsWith(QString::fromLatin1("min")))
        {
            if(dotPosition == -1)
                result = parse.mid(0, parse.length() - 3).toUInt() * 60;
            else
            {
                result = parse.mid(0, dotPosition).toUInt() * 60;
                QString temp = parse.mid(dotPosition + 1, parse.length() - dotPosition - 4);
                result += (60.0 * temp.toUInt()) * (1 / pow(10.0, int(temp.length())));
            }
        }
        else if(parse.endsWith(QString::fromLatin1("ms")))
        {
            if(dotPosition == -1)
                result = parse.mid(0, parse.length() - 2).toUInt() / 1000.0;
            else
            {
                result = parse.mid(0, dotPosition).toUInt() / 1000.0;
                QString temp = parse.mid(dotPosition + 1, parse.length() - dotPosition - 3);
                result += (temp.toUInt() / 1000.0) * (1 / pow(10.0, int(temp.length())));
            }
        }
        else if(parse.endsWith(QString::fromLatin1("s")))
        {
            if(dotPosition == -1)
                result = parse.mid(0, parse.length() - 1).toUInt();
            else
            {
                result = parse.mid(0, dotPosition).toUInt();
                QString temp = parse.mid(dotPosition + 1, parse.length() - dotPosition - 2);
                result += temp.toUInt() * (1 / pow(10.0, int(temp.length())));
            }
        }
        else
            result = parse.toDouble();
    }

    return result;
}

void SVGAnimationElementImpl::close()
{
    kdDebug() << " --> ADDING " << KDOM::DOMString(localName()) << " animation (startTime = " << getStartTime() << " ms) to scheduler!" << endl;
    SVGDocumentImpl *document = static_cast<SVGDocumentImpl *>(ownerDocument());
    if(!document)
        return;

    document->timeScheduler()->addTimer(this, qRound(getStartTime()));
}

KDOM::DOMString SVGAnimationElementImpl::targetAttribute() const
{
    if(!targetElement())
        return KDOM::DOMString();
    
    SVGStyledElementImpl *styled = dynamic_cast<SVGStyledElementImpl *>(targetElement());
    KDOM::CDFInterface *interface = (styled ? styled->ownerDocument()->implementation()->cdfInterface() : 0);
    
    KDOM::DOMString ret;

    EAttributeType attributeType = m_attributeType;
    if(attributeType == ATTRIBUTETYPE_AUTO)
    {
        attributeType = ATTRIBUTETYPE_XML;

        // Spec: The implementation should match the attributeName to an attribute
        // for the target element. The implementation must first search through the
        // list of CSS properties for a matching property name, and if none is found,
        // search the default XML namespace for the element.
        if(styled && styled->style() && interface)
        {
            int id = interface->getPropertyID(m_attributeName.ascii(), m_attributeName.length());
            if(styled->style()->getPropertyCSSValue(id))
                attributeType = ATTRIBUTETYPE_CSS;
        }
    }
    
    if(attributeType == ATTRIBUTETYPE_CSS)
    {
        if(styled && styled->style() && interface)
        {
            int id = interface->getPropertyID(m_attributeName.ascii(), m_attributeName.length());
            ret = KDOM::DOMString(styled->style()->getPropertyValue(id));
        }
    }

    if(attributeType == ATTRIBUTETYPE_XML || ret.isEmpty())
        ret = KDOM::DOMString(targetElement()->getAttribute(KDOM::DOMString(m_attributeName).handle()));

    return ret;
}

void SVGAnimationElementImpl::setTargetAttribute(const KDOM::DOMString &value)
{
    SVGAnimationElementImpl::setTargetAttribute(targetElement(), KDOM::DOMString(m_attributeName), value, m_attributeType);
}

void SVGAnimationElementImpl::setTargetAttribute(SVGElementImpl *target, const KDOM::DOMString &name, const KDOM::DOMString &value, EAttributeType type)
{
    if(!target)
        return;
    
    SVGStyledElementImpl *styled = dynamic_cast<SVGStyledElementImpl *>(target);
    KDOM::CDFInterface *interface = (styled ? styled->ownerDocument()->implementation()->cdfInterface() : 0);

    EAttributeType attributeType = type;
    if(type == ATTRIBUTETYPE_AUTO)
    {
        attributeType = ATTRIBUTETYPE_XML;

        // Spec: The implementation should match the attributeName to an attribute
        // for the target element. The implementation must first search through the
        // list of CSS properties for a matching property name, and if none is found,
        // search the default XML namespace for the element.
        if(styled && styled->style())
        {
            int id = interface->getPropertyID(name.string().ascii(), name.string().length());
            if(styled->style()->getPropertyCSSValue(id))
                attributeType = ATTRIBUTETYPE_CSS;
        }
    }
    
    if(attributeType == ATTRIBUTETYPE_CSS && styled && styled->style())
    {
        int id = interface->getPropertyID(name.string().ascii(), name.string().length());
        styled->style()->setProperty(id, value.handle());
    }
    else if(attributeType == ATTRIBUTETYPE_XML)
        target->setAttribute(name.handle(), value.handle());
}

QString SVGAnimationElementImpl::attributeName() const
{
    return m_attributeName;
}

bool SVGAnimationElementImpl::connected() const
{
    return m_connected;
}

bool SVGAnimationElementImpl::isFrozen() const
{
    return (m_fill == FILL_FREEZE);
}

bool SVGAnimationElementImpl::isAdditive() const
{
    return (m_additive == ADDITIVE_SUM) ||
           (detectAnimationMode() == BY_ANIMATION);
}

bool SVGAnimationElementImpl::isAccumulated() const
{
    return (m_accumulate == ACCUMULATE_SUM) &&
           (detectAnimationMode() != TO_ANIMATION);
}

EAnimationMode SVGAnimationElementImpl::detectAnimationMode() const
{
    if((!m_from.isEmpty() && !m_to.isEmpty()) || (!m_to.isEmpty())) // to/from-to animation
    {
        if(!m_from.isEmpty()) // from-to animation
            return FROM_TO_ANIMATION;
        else
            return TO_ANIMATION;
    }
    else if((m_from.isEmpty() && m_to.isEmpty() && !m_by.isEmpty()) ||
            (!m_from.isEmpty() && !m_by.isEmpty())) // by/from-by animation
    {
        if(!m_from.isEmpty()) // from-by animation
            return FROM_BY_ANIMATION;
        else
            return BY_ANIMATION;
    }
    else if(m_values)
        return VALUES_ANIMATION;

    return NO_ANIMATION;
}

int SVGAnimationElementImpl::calculateCurrentValueItem(double timePercentage)
{
    if(!m_values)
        return -1;
    
    unsigned long items = m_values->numberOfItems();

    // Calculate the relative time percentages for each 'fade'.
    double startTimes[items]; startTimes[0] = 0.0;
    for(unsigned int i = 1; i < items; ++i)
        startTimes[i] = (((2.0 * i)) / (items - 1)) / 2.0;

    int itemByPercentage = -1;
    for(unsigned int i = 0; i < items - 1; ++i)
    {
        if(timePercentage >= startTimes[i] && timePercentage <= startTimes[i + 1])
        {
            itemByPercentage = i;
            break;
        }
    }

    return itemByPercentage;
}

double SVGAnimationElementImpl::calculateRelativeTimePercentage(double timePercentage, int currentItem)
{
    if(currentItem == -1 || !m_values)
        return 0.0;

    unsigned long items = m_values->numberOfItems();

    // Calculate the relative time percentages for each 'fade'.
    double startTimes[items]; startTimes[0] = 0.0;
    for(unsigned int i = 1; i < items; ++i)
        startTimes[i] = (((2.0 * i)) / (items - 1)) / 2.0;

    double beginTimePercentage = startTimes[currentItem];
    double endTimePercentage = startTimes[currentItem + 1];

    if((endTimePercentage - beginTimePercentage) == 0.0)
        return 0.0;

    return ((timePercentage - beginTimePercentage) /
            (endTimePercentage - beginTimePercentage));
}

double SVGAnimationElementImpl::repeations() const
{
    return m_repeations;
}

bool SVGAnimationElementImpl::isIndefinite(double value) const
{
    return (value == DBL_MAX);
}

// vim:ts=4:noet
