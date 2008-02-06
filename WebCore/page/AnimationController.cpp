/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AnimationController.h"

#include "CSSPropertyNames.h"
#include "Document.h"
#include "FloatConversion.h"
#include "Frame.h"
#include "RenderObject.h"
#include "RenderStyle.h"
#include "SystemTime.h"
#include "Timer.h"

namespace WebCore {

static const double cAnimationTimerDelay = 0.025;

struct CurveData {
    CurveData(double p1x, double p1y, double p2x, double p2y)
    {
        // Calculate the polynomial coefficients, implicit first and last control points are (0,0) and (1,1).
        cx = 3.0 * p1x;
        bx = 3.0 * (p2x - p1x) - cx;
        ax = 1.0 - cx -bx;
         
        cy = 3.0 * p1y;
        by = 3.0 * (p2y - p1y) - cy;
        ay = 1.0 - cy - by;
    }
    
    double sampleCurveX(double t)
    {
        // `ax t^3 + bx t^2 + cx t' expanded using Horner's rule.
        return ((ax * t + bx) * t + cx) * t;
    }
    
    double sampleCurveY(double t)
    {
        return ((ay * t + by) * t + cy) * t;
    }
    
    double sampleCurveDerivativeX(double t)
    {
        return (3.0 * ax * t + 2.0 * bx) * t + cx;
    }
    
    // Given an x value, find a parametric value it came from.
    double solveCurveX(double x, double epsilon)
    {
        double t0;
        double t1;
        double t2;
        double x2;
        double d2;
        int i;

        // First try a few iterations of Newton's method -- normally very fast.
        for (t2 = x, i = 0; i < 8; i++) {
            x2 = sampleCurveX(t2) - x;
            if (fabs (x2) < epsilon)
                return t2;
            d2 = sampleCurveDerivativeX(t2);
            if (fabs(d2) < 1e-6)
                break;
            t2 = t2 - x2 / d2;
        }

        // Fall back to the bisection method for reliability.
        t0 = 0.0;
        t1 = 1.0;
        t2 = x;

        if (t2 < t0)
            return t0;
        if (t2 > t1)
            return t1;

        while (t0 < t1) {
            x2 = sampleCurveX(t2);
            if (fabs(x2 - x) < epsilon)
                return t2;
            if (x > x2)
                t0 = t2;
            else
                t1 = t2;
            t2 = (t1 - t0) * .5 + t0;
        }

        // Failure.
        return t2;
    }
    
private:
    double ax;
    double bx;
    double cx;
    
    double ay;
    double by;
    double cy;
};

// The epsilon value we pass to solveCurveX given that the animation is going to run over |dur| seconds. The longer the
// animation, the more precision we need in the timing function result to avoid ugly discontinuities.
static inline double solveEpsilon(double duration) { return 1. / (200. * duration); }

static inline double solveCubicBezierFunction(double p1x, double p1y, double p2x, double p2y, double t, double duration)
{
    // Convert from input time to parametric value in curve, then from
    // that to output time.
    CurveData c(p1x, p1y, p2x, p2y);
    t = c.solveCurveX(t, solveEpsilon(duration));
    t = c.sampleCurveY(t);
    return t;
}

class CompositeImplicitAnimation;

class ImplicitAnimation : public Noncopyable {
public:
    ImplicitAnimation(const Transition*);
    ~ImplicitAnimation();

    void animate(CompositeImplicitAnimation*, RenderObject*, RenderStyle* currentStyle, RenderStyle* targetStyle, RenderStyle*& animatedStyle);

    void reset(RenderObject*, RenderStyle* from, RenderStyle* to);
    
    double progress() const;

    bool finished() const { return m_finished; }

private:
    // The two styles that we are blending.
    RenderStyle* m_fromStyle;
    RenderStyle* m_toStyle;

    int m_property;
    TimingFunction m_function;
    double m_duration;

    int m_repeatCount;
    int m_iteration;
    
    bool m_finished;    
    double m_startTime;
    bool m_paused;
    double m_pauseTime;
};

class CompositeImplicitAnimation : public Noncopyable {
public:
    ~CompositeImplicitAnimation() { deleteAllValues(m_animations); }

    RenderStyle* animate(RenderObject*, RenderStyle* currentStyle, RenderStyle* targetStyle);

    bool animating() const;

    bool hasAnimationForProperty(int prop) const { return m_animations.contains(prop); }

    void reset(RenderObject*);

private:
    HashMap<int, ImplicitAnimation*> m_animations;
};

ImplicitAnimation::ImplicitAnimation(const Transition* transition)
: m_fromStyle(0)
, m_toStyle(0)
, m_property(transition->transitionProperty())
, m_function(transition->transitionTimingFunction())
, m_duration(transition->transitionDuration() / 1000.0)
, m_repeatCount(transition->transitionRepeatCount())
, m_iteration(0)
, m_finished(false)
, m_startTime(currentTime())
, m_paused(false)
, m_pauseTime(m_startTime)
{
}

ImplicitAnimation::~ImplicitAnimation()
{
    ASSERT(!m_fromStyle && !m_toStyle);
}

void ImplicitAnimation::reset(RenderObject* renderer, RenderStyle* from, RenderStyle* to)
{
    if (m_fromStyle)
        m_fromStyle->deref(renderer->renderArena());
    if (m_toStyle)
        m_toStyle->deref(renderer->renderArena());
    m_fromStyle = from;
    if (m_fromStyle)
        m_fromStyle->ref();
    m_toStyle = to;
    if (m_toStyle)
        m_toStyle->ref();
    m_finished = false;
    if (from || to)
        m_startTime = currentTime();
}

double ImplicitAnimation::progress() const
{
    double elapsedTime = currentTime() - m_startTime;
    
    if (m_finished || !m_duration || elapsedTime >= m_duration)
        return 1.0;

    if (m_function.type() == LinearTimingFunction)
        return elapsedTime / m_duration;
    
    // Cubic bezier.
    return solveCubicBezierFunction(m_function.x1(), m_function.y1(), 
                                    m_function.x2(), m_function.y2(),
                                    elapsedTime / m_duration, m_duration);
}

static inline int blendFunc(int from, int to, double progress)
{  
    return int(from + (to - from) * progress);
}

static inline double blendFunc(double from, double to, double progress)
{  
    return from + (to - from) * progress;
}

static inline float blendFunc(float from, float to, double progress)
{  
    return narrowPrecisionToFloat(from + (to - from) * progress);
}

static inline Color blendFunc(const Color& from, const Color& to, double progress)
{  
    return Color(blendFunc(from.red(), to.red(), progress),
                 blendFunc(from.green(), to.green(), progress),
                 blendFunc(from.blue(), to.blue(), progress),
                 blendFunc(from.alpha(), to.alpha(), progress));
}

static inline Length blendFunc(const Length& from, const Length& to, double progress)
{  
    return to.blend(from, progress);
}

static inline IntSize blendFunc(const IntSize& from, const IntSize& to, double progress)
{  
    return IntSize(blendFunc(from.width(), to.width(), progress),
                   blendFunc(from.height(), to.height(), progress));
}

static inline ShadowData* blendFunc(const ShadowData* from, const ShadowData* to, double progress)
{  
    ASSERT(from && to);
    return new ShadowData(blendFunc(from->x, to->x, progress), blendFunc(from->y, to->y, progress), blendFunc(from->blur, to->blur, progress), blendFunc(from->color, to->color, progress));
}

static inline TransformOperations blendFunc(const TransformOperations& from, const TransformOperations& to, double progress)
{    
    // Blend any operations whose types actually match up.  Otherwise don't bother.
    unsigned fromSize = from.size();
    unsigned toSize = to.size();
    unsigned size = max(fromSize, toSize);
    TransformOperations result;
    for (unsigned i = 0; i < size; i++) {
        TransformOperation* fromOp = i < fromSize ? from[i].get() : 0;
        TransformOperation* toOp = i < toSize ? to[i].get() : 0;
        TransformOperation* blendedOp = toOp ? toOp->blend(fromOp, progress) : fromOp->blend(0, progress, true);
        result.append(blendedOp);
    }
    return result;
}

static inline EVisibility blendFunc(EVisibility from, EVisibility to, double progress)
{
    // Any non-zero result means we consider the object to be visible.  Only at 0 do we consider the object to be
    // invisible.   The invisible value we use (HIDDEN vs. COLLAPSE) depends on the specified from/to values.
    double fromVal = from == VISIBLE ? 1. : 0.;
    double toVal = to == VISIBLE ? 1. : 0.;
    if (fromVal == toVal)
        return to;
    double result = blendFunc(fromVal, toVal, progress);
    return result > 0. ? VISIBLE : (to != VISIBLE ? to : from);
}

#define BLEND(prop, getter, setter) \
    if (m_property == prop && m_toStyle->getter() != targetStyle->getter()) \
        reset(renderer, currentStyle, targetStyle); \
    \
    if ((m_property == cAnimateAll && !animation->hasAnimationForProperty(prop)) || m_property == prop) { \
        if (m_fromStyle->getter() != m_toStyle->getter()) {\
            m_finished = false; \
            if (!animatedStyle) \
                animatedStyle = new (renderer->renderArena()) RenderStyle(*targetStyle); \
            animatedStyle->setter(blendFunc(m_fromStyle->getter(), m_toStyle->getter(), progress()));\
            if (m_property == prop) \
                return; \
        }\
    }\

#define BLEND_MAYBE_INVALID_COLOR(prop, getter, setter) \
    if (m_property == prop && m_toStyle->getter() != targetStyle->getter()) \
        reset(renderer, currentStyle, targetStyle); \
    \
    if ((m_property == cAnimateAll && !animation->hasAnimationForProperty(prop)) || m_property == prop) { \
        Color fromColor = m_fromStyle->getter(); \
        Color toColor = m_toStyle->getter(); \
        if (!fromColor.isValid()) \
            fromColor = m_fromStyle->color(); \
        if (!toColor.isValid()) \
            toColor = m_toStyle->color(); \
        if (fromColor != toColor) {\
            m_finished = false; \
            if (!animatedStyle) \
                animatedStyle = new (renderer->renderArena()) RenderStyle(*targetStyle); \
            animatedStyle->setter(blendFunc(fromColor, toColor, progress()));\
            if (m_property == prop) \
                return; \
        }\
    }\

#define BLEND_SHADOW(prop, getter, setter) \
    if (m_property == prop && (!m_toStyle->getter() || !targetStyle->getter() || *m_toStyle->getter() != *targetStyle->getter())) \
        reset(renderer, currentStyle, targetStyle); \
    \
    if ((m_property == cAnimateAll && !animation->hasAnimationForProperty(prop)) || m_property == prop) { \
        if (m_fromStyle->getter() && m_toStyle->getter() && *m_fromStyle->getter() != *m_toStyle->getter()) {\
            m_finished = false; \
            if (!animatedStyle) \
                animatedStyle = new (renderer->renderArena()) RenderStyle(*targetStyle); \
            animatedStyle->setter(blendFunc(m_fromStyle->getter(), m_toStyle->getter(), progress()));\
            if (m_property == prop) \
                return; \
        }\
    }

void ImplicitAnimation::animate(CompositeImplicitAnimation* animation, RenderObject* renderer, RenderStyle* currentStyle, RenderStyle* targetStyle, RenderStyle*& animatedStyle)
{
    // FIXME: If we have no transition-property, then the only way to tell if our goal state changed is to check
    // every single animatable property.  For now we'll just diff the styles to ask that question,
    // but we should really exclude non-animatable properties.
    if (!m_toStyle || (m_property == cAnimateAll && targetStyle->diff(m_toStyle)))
        reset(renderer, currentStyle, targetStyle);

    // FIXME: Blow up shorthands so that they can be honored.
    m_finished = true;
    BLEND(CSS_PROP_LEFT, left, setLeft);
    BLEND(CSS_PROP_RIGHT, right, setRight);
    BLEND(CSS_PROP_TOP, top, setTop);
    BLEND(CSS_PROP_BOTTOM, bottom, setBottom);
    BLEND(CSS_PROP_WIDTH, width, setWidth);
    BLEND(CSS_PROP_HEIGHT, height, setHeight);
    BLEND(CSS_PROP_BORDER_LEFT_WIDTH, borderLeftWidth, setBorderLeftWidth);
    BLEND(CSS_PROP_BORDER_RIGHT_WIDTH, borderRightWidth, setBorderRightWidth);
    BLEND(CSS_PROP_BORDER_TOP_WIDTH, borderTopWidth, setBorderTopWidth);
    BLEND(CSS_PROP_BORDER_BOTTOM_WIDTH, borderBottomWidth, setBorderBottomWidth);
    BLEND(CSS_PROP_MARGIN_LEFT, marginLeft, setMarginLeft);
    BLEND(CSS_PROP_MARGIN_RIGHT, marginRight, setMarginRight);
    BLEND(CSS_PROP_MARGIN_TOP, marginTop, setMarginTop);
    BLEND(CSS_PROP_MARGIN_BOTTOM, marginBottom, setMarginBottom);
    BLEND(CSS_PROP_PADDING_LEFT, paddingLeft, setPaddingLeft);
    BLEND(CSS_PROP_PADDING_RIGHT, paddingRight, setPaddingRight);
    BLEND(CSS_PROP_PADDING_TOP, paddingTop, setPaddingTop);
    BLEND(CSS_PROP_PADDING_BOTTOM, paddingBottom, setPaddingBottom);
    BLEND(CSS_PROP_OPACITY, opacity, setOpacity);
    BLEND(CSS_PROP_COLOR, color, setColor);
    BLEND(CSS_PROP_BACKGROUND_COLOR, backgroundColor, setBackgroundColor);
    BLEND_MAYBE_INVALID_COLOR(CSS_PROP__WEBKIT_COLUMN_RULE_COLOR, columnRuleColor, setColumnRuleColor);
    BLEND(CSS_PROP__WEBKIT_COLUMN_RULE_WIDTH, columnRuleWidth, setColumnRuleWidth);
    BLEND(CSS_PROP__WEBKIT_COLUMN_GAP, columnGap, setColumnGap);
    BLEND(CSS_PROP__WEBKIT_COLUMN_COUNT, columnCount, setColumnCount);
    BLEND(CSS_PROP__WEBKIT_COLUMN_WIDTH, columnWidth, setColumnWidth);
    BLEND_MAYBE_INVALID_COLOR(CSS_PROP__WEBKIT_TEXT_STROKE_COLOR, textStrokeColor, setTextStrokeColor);
    BLEND_MAYBE_INVALID_COLOR(CSS_PROP__WEBKIT_TEXT_FILL_COLOR, textFillColor, setTextFillColor);
    BLEND(CSS_PROP__WEBKIT_BORDER_HORIZONTAL_SPACING, horizontalBorderSpacing, setHorizontalBorderSpacing);
    BLEND(CSS_PROP__WEBKIT_BORDER_VERTICAL_SPACING, verticalBorderSpacing, setVerticalBorderSpacing);
    BLEND_MAYBE_INVALID_COLOR(CSS_PROP_BORDER_LEFT_COLOR, borderLeftColor, setBorderLeftColor);
    BLEND_MAYBE_INVALID_COLOR(CSS_PROP_BORDER_RIGHT_COLOR, borderRightColor, setBorderRightColor);
    BLEND_MAYBE_INVALID_COLOR(CSS_PROP_BORDER_TOP_COLOR, borderTopColor, setBorderTopColor);
    BLEND_MAYBE_INVALID_COLOR(CSS_PROP_BORDER_BOTTOM_COLOR, borderBottomColor, setBorderBottomColor);
    BLEND(CSS_PROP_Z_INDEX, zIndex, setZIndex);
    BLEND(CSS_PROP_LINE_HEIGHT, lineHeight, setLineHeight);
    BLEND_MAYBE_INVALID_COLOR(CSS_PROP_OUTLINE_COLOR, outlineColor, setOutlineColor);
    BLEND(CSS_PROP_OUTLINE_OFFSET, outlineOffset, setOutlineOffset);
    BLEND(CSS_PROP_OUTLINE_WIDTH, outlineWidth, setOutlineWidth);
    BLEND(CSS_PROP_LETTER_SPACING, letterSpacing, setLetterSpacing);
    BLEND(CSS_PROP_WORD_SPACING, wordSpacing, setWordSpacing);
    BLEND_SHADOW(CSS_PROP__WEBKIT_BOX_SHADOW, boxShadow, setBoxShadow);
    BLEND_SHADOW(CSS_PROP_TEXT_SHADOW, textShadow, setTextShadow);
    BLEND(CSS_PROP__WEBKIT_TRANSFORM, transform, setTransform);
    BLEND(CSS_PROP__WEBKIT_TRANSFORM_ORIGIN_X, transformOriginX, setTransformOriginX);
    BLEND(CSS_PROP__WEBKIT_TRANSFORM_ORIGIN_Y, transformOriginY, setTransformOriginY);
    BLEND(CSS_PROP__WEBKIT_BORDER_TOP_LEFT_RADIUS, borderTopLeftRadius, setBorderTopLeftRadius);
    BLEND(CSS_PROP__WEBKIT_BORDER_TOP_RIGHT_RADIUS, borderTopRightRadius, setBorderTopRightRadius);
    BLEND(CSS_PROP__WEBKIT_BORDER_BOTTOM_LEFT_RADIUS, borderBottomLeftRadius, setBorderBottomLeftRadius);
    BLEND(CSS_PROP__WEBKIT_BORDER_BOTTOM_RIGHT_RADIUS, borderBottomRightRadius, setBorderBottomRightRadius);
    BLEND(CSS_PROP_VISIBILITY, visibility, setVisibility);
}

RenderStyle* CompositeImplicitAnimation::animate(RenderObject* renderer, RenderStyle* currentStyle, RenderStyle* targetStyle)
{
    // Get the animation layers from the target style.
    // For each one, we need to create a new animation unless one exists already (later occurrences of duplicate
    // triggers in the layer list get ignored).
    if (m_animations.isEmpty()) {
        for (const Transition* transition = targetStyle->transitions(); transition; transition = transition->next()) {
            int property = transition->transitionProperty();
            int duration = transition->transitionDuration();
            int repeatCount = transition->transitionRepeatCount();
            if (property && duration && repeatCount && !m_animations.contains(property)) {
                ImplicitAnimation* animation = new ImplicitAnimation(transition);
                m_animations.set(property, animation);
            }
        }
    }

    // Now that we have animation objects ready, let them know about the new goal state.  We want them
    // to fill in a RenderStyle*& only if needed.
    RenderStyle* result = 0;
    HashMap<int, ImplicitAnimation*>::iterator end = m_animations.end();
    for (HashMap<int, ImplicitAnimation*>::iterator it = m_animations.begin(); it != end; ++it)
        it->second->animate(this, renderer, currentStyle, targetStyle, result);
    
    if (result)
        return result;

    return targetStyle;
}

bool CompositeImplicitAnimation::animating() const
{
    HashMap<int, ImplicitAnimation*>::const_iterator end = m_animations.end();
    for (HashMap<int, ImplicitAnimation*>::const_iterator it = m_animations.begin(); it != end; ++it)
        if (!it->second->finished())
            return true;
    return false;
}

void CompositeImplicitAnimation::reset(RenderObject* renderer)
{
    HashMap<int, ImplicitAnimation*>::const_iterator end = m_animations.end();
    for (HashMap<int, ImplicitAnimation*>::const_iterator it = m_animations.begin(); it != end; ++it)
        it->second->reset(renderer, 0, 0);
}

class AnimationControllerPrivate {
public:
    AnimationControllerPrivate(Frame*);
    ~AnimationControllerPrivate();

    CompositeImplicitAnimation* get(RenderObject*);
    bool clear(RenderObject*);
    
    void timerFired(Timer<AnimationControllerPrivate>*);
    void updateTimer();

    bool hasImplicitAnimations() const { return !m_animations.isEmpty(); }

private:
    HashMap<RenderObject*, CompositeImplicitAnimation*> m_animations;
    Timer<AnimationControllerPrivate> m_timer;
    Frame* m_frame;
};

AnimationControllerPrivate::AnimationControllerPrivate(Frame* frame)
    : m_timer(this, &AnimationControllerPrivate::timerFired)
    , m_frame(frame)
{
}

AnimationControllerPrivate::~AnimationControllerPrivate()
{
    deleteAllValues(m_animations);
}

CompositeImplicitAnimation* AnimationControllerPrivate::get(RenderObject* renderer)
{
    CompositeImplicitAnimation* animation = m_animations.get(renderer);
    if (!animation) {
        animation = new CompositeImplicitAnimation();
        m_animations.set(renderer, animation);
    }
    return animation;
}

bool AnimationControllerPrivate::clear(RenderObject* renderer)
{
    CompositeImplicitAnimation* animation = m_animations.take(renderer);
    if (!animation)
        return false;
    animation->reset(renderer);
    delete animation;
    return true;
}

void AnimationControllerPrivate::updateTimer()
{
    bool animating = false;
    HashMap<RenderObject*, CompositeImplicitAnimation*>::iterator end = m_animations.end();
    for (HashMap<RenderObject*, CompositeImplicitAnimation*>::iterator it = m_animations.begin(); it != end; ++it) {
        if (it->second->animating()) {
            animating = true;
            break;
        }
    }
    
    if (animating) {
        if (!m_timer.isActive())
            m_timer.startRepeating(cAnimationTimerDelay);
    } else if (m_timer.isActive())
        m_timer.stop();
}

void AnimationControllerPrivate::timerFired(Timer<AnimationControllerPrivate>* timer)
{
    // When the timer fires, all we do is call setChanged on all DOM nodes with running animations and then do an immediate
    // updateRendering.  It will then call back to us with new information.
    bool animating = false;
    HashMap<RenderObject*, CompositeImplicitAnimation*>::iterator end = m_animations.end();
    for (HashMap<RenderObject*, CompositeImplicitAnimation*>::iterator it = m_animations.begin(); it != end; ++it) {
        if (it->second->animating()) {
            animating = true;
            it->first->element()->setChanged();
        }
    }
    
    m_frame->document()->updateRendering();
    
    updateTimer();
}

AnimationController::AnimationController(Frame* frame)
:m_data(new AnimationControllerPrivate(frame))
{

}

AnimationController::~AnimationController()
{
    delete m_data;
}

void AnimationController::cancelImplicitAnimations(RenderObject* renderer)
{
    if (!m_data->hasImplicitAnimations())
        return;

    if (m_data->clear(renderer))
        renderer->element()->setChanged();
}

RenderStyle* AnimationController::updateImplicitAnimations(RenderObject* renderer, RenderStyle* newStyle)
{
    // Fetch our current set of implicit animations from a hashtable.  We then compare them
    // against the animations in the style and make sure we're in sync.  If destination values
    // have changed, we reset the animation.  We then do a blend to get new values and we return
    // a new style.
    ASSERT(renderer->element()); // FIXME: We do not animate generated content yet.
    
    CompositeImplicitAnimation* animation = m_data->get(renderer);
    RenderStyle* result = animation->animate(renderer, renderer->style(), newStyle);
    m_data->updateTimer();
    return result;
}

void AnimationController::suspendAnimations()
{
    // FIXME: Walk the whole hashtable and call pause on each animation.
    // Kill our timer.
}

void AnimationController::resumeAnimations()
{
    // FIXME: Walk the whole hashtable and call resume on each animation.
    // Start our timer.
}

}
