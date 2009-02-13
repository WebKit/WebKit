/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "GraphicsLayer.h"

#include "FloatPoint.h"
#include "RotateTransformOperation.h"
#include "TextStream.h"

namespace WebCore {

void GraphicsLayer::FloatValue::set(float key, float value, const TimingFunction* timingFunction)
{
    m_key = key;
    m_value = value;
    if (timingFunction != m_timingFunction) {
        if (timingFunction)
            m_timingFunction.set(new TimingFunction(*timingFunction));
        else
            m_timingFunction.clear();
    }
}

void GraphicsLayer::TransformValue::set(float key, const TransformOperations* value, const TimingFunction* timingFunction)
{
    m_key = key;
    if (value != m_value) {
        if (value)
            m_value.set(new TransformOperations(*value));
        else
            m_value.clear();
    }
    if (timingFunction != m_timingFunction) {
        if (timingFunction)
            m_timingFunction.set(new TimingFunction(*timingFunction));
        else
            m_timingFunction.clear();
    }
}

void GraphicsLayer::FloatValueList::insert(float key, float value, const TimingFunction* timingFunction)
{
    for (size_t i = 0; i < m_values.size(); ++i) {
        FloatValue& curFloatValue = m_values[i];
        if (curFloatValue.key() == key) {
            curFloatValue.set(key, value, timingFunction);
            return;
        }
        if (curFloatValue.key() > key) {
            // insert before
            m_values.insert(i, FloatValue(key, value, timingFunction));
            return;
        }
    }
    
    // append
    m_values.append(FloatValue(key, value, timingFunction));
}

void GraphicsLayer::TransformValueList::insert(float key, const TransformOperations* value, const TimingFunction* timingFunction)
{
    for (size_t i = 0; i < m_values.size(); ++i) {
        TransformValue& curTransValue = m_values[i];
        if (curTransValue.key() == key) {
            curTransValue.set(key, value, timingFunction);
            return;
        }
        if (curTransValue.key() > key) {
            // insert before
            m_values.insert(i, TransformValue(key, value, timingFunction));
            return;
        }
    }
    
    // append
    m_values.append(TransformValue(key, value, timingFunction));
}

// An "invalid" list is one whose functions don't match, and therefore has to be animated as a Matrix
// The hasBigRotation flag will always return false if isValid is false. Otherwise hasBigRotation is 
// true if the rotation between any two keyframes is >= 180 degrees.
void GraphicsLayer::TransformValueList::makeFunctionList(FunctionList& list, bool& isValid, bool& hasBigRotation) const
{
    list.clear();
    isValid = false;
    hasBigRotation = false;
    
    if (m_values.size() < 2)
        return;
    
    // empty transforms match anything, so find the first non-empty entry as the reference
    size_t firstIndex = 0;
    for ( ; firstIndex < m_values.size(); ++firstIndex) {
        if (m_values[firstIndex].value()->operations().size() > 0)
            break;
    }
    
    if (firstIndex >= m_values.size())
        return;
        
    const TransformOperations* firstVal = m_values[firstIndex].value();
    
    // see if the keyframes are valid
    for (size_t i = firstIndex + 1; i < m_values.size(); ++i) {
        const TransformOperations* val = m_values[i].value();
        
        // a null transform matches anything
        if (val->operations().isEmpty())
            continue;
            
        if (firstVal->operations().size() != val->operations().size())
            return;
            
        for (size_t j = 0; j < firstVal->operations().size(); ++j) {
            if (!firstVal->operations().at(j)->isSameType(*val->operations().at(j)))
                return;
        }
    }

    // keyframes are valid, fill in the list
    isValid = true;
    
    double lastRotAngle = 0.0;
    double maxRotAngle = -1.0;
        
    list.resize(firstVal->operations().size());
    for (size_t j = 0; j < firstVal->operations().size(); ++j) {
        TransformOperation::OperationType type = firstVal->operations().at(j)->getOperationType();
        list[j] = type;
        
        // if this is a rotation entry, we need to see if any angle differences are >= 180 deg
        if (type == TransformOperation::ROTATE_X ||
            type == TransformOperation::ROTATE_Y ||
            type == TransformOperation::ROTATE_Z ||
            type == TransformOperation::ROTATE_3D) {
            lastRotAngle = static_cast<RotateTransformOperation*>(firstVal->operations().at(j).get())->angle();
            
            if (maxRotAngle < 0)
                maxRotAngle = fabs(lastRotAngle);
            
            for (size_t i = firstIndex + 1; i < m_values.size(); ++i) {
                const TransformOperations* val = m_values[i].value();
                double rotAngle = val->operations().isEmpty() ? 0 : (static_cast<RotateTransformOperation*>(val->operations().at(j).get())->angle());
                double diffAngle = fabs(rotAngle - lastRotAngle);
                if (diffAngle > maxRotAngle)
                    maxRotAngle = diffAngle;
                lastRotAngle = rotAngle;
            }
        }
    }
    
    hasBigRotation = maxRotAngle >= 180.0;
}

GraphicsLayer::GraphicsLayer(GraphicsLayerClient* client)
    : m_client(client)
    , m_anchorPoint(0.5f, 0.5f, 0)
    , m_opacity(1)
#ifndef NDEBUG
    , m_zPosition(0)
#endif
    , m_backgroundColorSet(false)
    , m_contentsOpaque(false)
    , m_preserves3D(false)
    , m_backfaceVisibility(true)
    , m_usingTiledLayer(false)
    , m_masksToBounds(false)
    , m_drawsContent(false)
    , m_paintingPhase(GraphicsLayerPaintAllMask)
    , m_parent(0)
#ifndef NDEBUG
    , m_repaintCount(0)
#endif
{
}

GraphicsLayer::~GraphicsLayer()
{
    removeAllAnimations();    

    removeAllChildren();
    removeFromParent();
}

void GraphicsLayer::addChild(GraphicsLayer* childLayer)
{
    ASSERT(childLayer != this);
    
    if (childLayer->parent())
        childLayer->removeFromParent();

    childLayer->setParent(this);
    m_children.append(childLayer);
}

void GraphicsLayer::addChildAtIndex(GraphicsLayer* childLayer, int index)
{
    ASSERT(childLayer != this);

    if (childLayer->parent())
        childLayer->removeFromParent();

    childLayer->setParent(this);
    m_children.insert(index, childLayer);
}

void GraphicsLayer::addChildBelow(GraphicsLayer* childLayer, GraphicsLayer* sibling)
{
    ASSERT(childLayer != this);
    childLayer->removeFromParent();

    bool found = false;
    for (unsigned i = 0; i < m_children.size(); i++) {
        if (sibling == m_children[i]) {
            m_children.insert(i, childLayer);
            found = true;
            break;
        }
    }

    childLayer->setParent(this);

    if (!found)
        m_children.append(childLayer);
}

void GraphicsLayer::addChildAbove(GraphicsLayer* childLayer, GraphicsLayer* sibling)
{
    childLayer->removeFromParent();
    ASSERT(childLayer != this);

    bool found = false;
    for (unsigned i = 0; i < m_children.size(); i++) {
        if (sibling == m_children[i]) {
            m_children.insert(i+1, childLayer);
            found = true;
            break;
        }
    }

    childLayer->setParent(this);

    if (!found)
        m_children.append(childLayer);
}

bool GraphicsLayer::replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild)
{
    ASSERT(!newChild->parent());
    bool found = false;
    for (unsigned i = 0; i < m_children.size(); i++) {
        if (oldChild == m_children[i]) {
            m_children[i] = newChild;
            found = true;
            break;
        }
    }
    if (found) {
        oldChild->setParent(0);

        newChild->removeFromParent();
        newChild->setParent(this);
        return true;
    }
    return false;
}

void GraphicsLayer::removeAllChildren()
{
    while (m_children.size()) {
        GraphicsLayer* curLayer = m_children[0];
        ASSERT(curLayer->parent());
        curLayer->removeFromParent();
    }
}

void GraphicsLayer::removeFromParent()
{
    if (m_parent) {
        unsigned i;
        for (i = 0; i < m_parent->m_children.size(); i++) {
            if (this == m_parent->m_children[i]) {
                m_parent->m_children.remove(i);
                break;
            }
        }

        setParent(0);
    }
}

void GraphicsLayer::setBackgroundColor(const Color& inColor, const Animation*, double /*beginTime*/)
{
    m_backgroundColor = inColor;
    m_backgroundColorSet = true;
}

void GraphicsLayer::clearBackgroundColor()
{
    m_backgroundColor = Color();
    m_backgroundColorSet = false;
}

bool GraphicsLayer::setOpacity(float opacity, const Animation*, double)
{
    m_opacity = opacity;
    return false;       // not animating
}

void GraphicsLayer::paintGraphicsLayerContents(GraphicsContext& context, const IntRect& clip)
{
    m_client->paintContents(this, context, m_paintingPhase, clip);
}

String GraphicsLayer::propertyIdToString(AnimatedPropertyID property)
{
    switch (property) {
        case AnimatedPropertyWebkitTransform:
            return "transform";
        case AnimatedPropertyOpacity:
            return "opacity";
        case AnimatedPropertyBackgroundColor:
            return "backgroundColor";
        case AnimatedPropertyInvalid:
            ASSERT_NOT_REACHED();
    }
    ASSERT_NOT_REACHED();
    return "";
}

int GraphicsLayer::findAnimationEntry(AnimatedPropertyID property, short index) const
{
    for (size_t i = 0; i < m_animations.size(); ++i) {
        if (m_animations[i].matches(property, index))
            return static_cast<int>(i);
    }
    return -1;
}

void GraphicsLayer::addAnimationEntry(AnimatedPropertyID property, short index, bool isTransition, const Animation* transition)
{
    int i = findAnimationEntry(property, index);
    
    if (i >= 0)
        m_animations[i].reset(transition, isTransition);
    else
        m_animations.append(AnimationEntry(transition, property, index, isTransition));
}

void GraphicsLayer::removeAllAnimations()
{
    size_t size = m_animations.size();
    for (size_t i = 0; i < size; ++i)
        removeAnimation(0, true);
}

void GraphicsLayer::removeAllAnimationsForProperty(AnimatedPropertyID property)
{
    for (short j = 0; ; ++j) {
        int i = findAnimationEntry(property, j);
        if (i < 0)
            break;
        removeAnimation(i, false);
    }
}

void GraphicsLayer::removeFinishedAnimations(const String& name, int /*index*/, bool reset)
{
    size_t size = m_animations.size();
    for (size_t i = 0; i < size; ) {
        AnimationEntry& anim = m_animations[i];
        if (!anim.isTransition() && anim.animation()->name() == name) {
            removeAnimation(i, reset);
            --size;
        } else
            ++i;
    }
}

void GraphicsLayer::removeFinishedTransitions(AnimatedPropertyID property)
{
    size_t size = m_animations.size();
    for (size_t i = 0; i < size; ) {
        AnimationEntry& anim = m_animations[i];
        if (anim.isTransition() && property == anim.property()) {
            removeAnimation(i, false);
            --size;
        } else
            ++i;
    }
}

void GraphicsLayer::suspendAnimations()
{
}

void GraphicsLayer::resumeAnimations()
{
}

#ifndef NDEBUG
void GraphicsLayer::updateDebugIndicators()
{
    if (GraphicsLayer::showDebugBorders()) {
        if (drawsContent()) {
            if (m_usingTiledLayer)
                setDebugBorder(Color(0, 255, 0, 204), 2.0f);    // tiled layer: green
            else
                setDebugBorder(Color(255, 0, 0, 204), 2.0f);    // normal layer: red
        } else if (masksToBounds()) {
            setDebugBorder(Color(128, 255, 255, 178), 2.0f);    // masking layer: pale blue
            if (GraphicsLayer::showDebugBorders())
                setDebugBackgroundColor(Color(128, 255, 255, 52));
        } else
            setDebugBorder(Color(255, 255, 0, 204), 2.0f);      // container: yellow
    }
}

void GraphicsLayer::setZPosition(float position)
{
    m_zPosition = position;
}
#endif

static void writeIndent(TextStream& ts, int indent)
{
    for (int i = 0; i != indent; ++i)
        ts << "  ";
}

void GraphicsLayer::dumpLayer(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "(" << "GraphicsLayer" << " " << static_cast<void*>(const_cast<GraphicsLayer*>(this));
    ts << " \"" << m_name << "\"\n";
    dumpProperties(ts, indent);
    writeIndent(ts, indent);
    ts << ")\n";
}

void GraphicsLayer::dumpProperties(TextStream& ts, int indent) const
{
    writeIndent(ts, indent + 1);
    ts << "(position " << m_position.x() << " " << m_position.y() << ")\n";

    writeIndent(ts, indent + 1);
    ts << "(anchor " << m_anchorPoint.x() << " " << m_anchorPoint.y() << ")\n";

    writeIndent(ts, indent + 1);
    ts << "(bounds " << m_size.width() << " " << m_size.height() << ")\n";

    writeIndent(ts, indent + 1);
    ts << "(opacity " << m_opacity << ")\n";
    
    writeIndent(ts, indent + 1);
    ts << "(usingTiledLayer " << m_usingTiledLayer << ")\n";

    writeIndent(ts, indent + 1);
    ts << "(m_preserves3D " << m_preserves3D << ")\n";

    writeIndent(ts, indent + 1);
    ts << "(drawsContent " << m_drawsContent << ")\n";

    writeIndent(ts, indent + 1);
    ts << "(m_backfaceVisibility " << (m_backfaceVisibility ? "visible" : "hidden") << ")\n";

    writeIndent(ts, indent + 1);
    ts << "(client ";
    if (m_client)
        ts << static_cast<void*>(m_client);
    else
        ts << "none";
    ts << ")\n";

    writeIndent(ts, indent + 1);
    ts << "(backgroundColor ";
    if (!m_backgroundColorSet)
        ts << "none";
    else
        ts << m_backgroundColor.name();
    ts << ")\n";

    writeIndent(ts, indent + 1);
    ts << "(transform ";
    if (m_transform.isIdentity())
        ts << "identity";
    else {
        ts << "[" << m_transform.m11() << " " << m_transform.m12() << " " << m_transform.m13() << " " << m_transform.m14() << "] ";
        ts << "[" << m_transform.m21() << " " << m_transform.m22() << " " << m_transform.m23() << " " << m_transform.m24() << "] ";
        ts << "[" << m_transform.m31() << " " << m_transform.m32() << " " << m_transform.m33() << " " << m_transform.m34() << "] ";
        ts << "[" << m_transform.m41() << " " << m_transform.m42() << " " << m_transform.m43() << " " << m_transform.m44() << "]";
    }
    ts << ")\n";

    writeIndent(ts, indent + 1);
    ts << "(childrenTransform ";
    if (m_childrenTransform.isIdentity())
        ts << "identity";
    else {
        ts << "[" << m_childrenTransform.m11() << " " << m_childrenTransform.m12() << " " << m_childrenTransform.m13() << " " << m_childrenTransform.m14() << "] ";
        ts << "[" << m_childrenTransform.m21() << " " << m_childrenTransform.m22() << " " << m_childrenTransform.m23() << " " << m_childrenTransform.m24() << "] ";
        ts << "[" << m_childrenTransform.m31() << " " << m_childrenTransform.m32() << " " << m_childrenTransform.m33() << " " << m_childrenTransform.m34() << "] ";
        ts << "[" << m_childrenTransform.m41() << " " << m_childrenTransform.m42() << " " << m_childrenTransform.m43() << " " << m_childrenTransform.m44() << "]";
    }
    ts << ")\n";

    writeIndent(ts, indent + 1);
    ts << "(children " << m_children.size() << "\n";
    
    unsigned i;
    for (i = 0; i < m_children.size(); i++)
        m_children[i]->dumpLayer(ts, indent+2);
    writeIndent(ts, indent + 1);
    ts << ")\n";
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
