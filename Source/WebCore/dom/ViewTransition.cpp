/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "ViewTransition.h"

#include "CSSKeyframeRule.h"
#include "CSSKeyframesRule.h"
#include "CSSTransformListValue.h"
#include "CheckVisibilityOptions.h"
#include "ComputedStyleExtractor.h"
#include "Document.h"
#include "DocumentTimeline.h"
#include "FrameSnapshotting.h"
#include "HostWindow.h"
#include "JSDOMPromise.h"
#include "JSDOMPromiseDeferred.h"
#include "LayoutRect.h"
#include "PseudoElementRequest.h"
#include "RenderBox.h"
#include "RenderLayer.h"
#include "RenderLayerModelObject.h"
#include "RenderView.h"
#include "RenderViewTransitionCapture.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include "Styleable.h"
#include "WebAnimation.h"

namespace WebCore {

static std::pair<Ref<DOMPromise>, Ref<DeferredPromise>> createPromiseAndWrapper(Document& document)
{
    auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(document.globalObject());
    JSC::JSLockHolder lock(globalObject.vm());
    RefPtr deferredPromise = DeferredPromise::create(globalObject);
    Ref domPromise = DOMPromise::create(globalObject, *JSC::jsCast<JSC::JSPromise*>(deferredPromise->promise()));
    return { WTFMove(domPromise), deferredPromise.releaseNonNull() };
}

ViewTransition::ViewTransition(Document& document, RefPtr<ViewTransitionUpdateCallback>&& updateCallback)
    : ActiveDOMObject(document)
    , m_updateCallback(WTFMove(updateCallback))
    , m_ready(createPromiseAndWrapper(document))
    , m_updateCallbackDone(createPromiseAndWrapper(document))
    , m_finished(createPromiseAndWrapper(document))
{
}

ViewTransition::~ViewTransition() = default;

Ref<ViewTransition> ViewTransition::create(Document& document, RefPtr<ViewTransitionUpdateCallback>&& updateCallback)
{
    Ref viewTransition = adoptRef(*new ViewTransition(document, WTFMove(updateCallback)));
    viewTransition->suspendIfNeeded();
    return viewTransition;
}

DOMPromise& ViewTransition::ready()
{
    return m_ready.first.get();
}

DOMPromise& ViewTransition::updateCallbackDone()
{
    return m_updateCallbackDone.first.get();
}

DOMPromise& ViewTransition::finished()
{
    return m_finished.first.get();
}

// https://drafts.csswg.org/css-view-transitions/#skip-the-view-transition
void ViewTransition::skipViewTransition(ExceptionOr<JSC::JSValue>&& reason)
{
    if (!document())
        return;

    ASSERT(m_phase != ViewTransitionPhase::Done);

    if (m_phase < ViewTransitionPhase::UpdateCallbackCalled) {
        protectedDocument()->checkedEventLoop()->queueTask(TaskSource::DOMManipulation, [this, weakThis = WeakPtr { *this }] {
            RefPtr protectedThis = weakThis.get();
            if (protectedThis)
                callUpdateCallback();
        });
    }

    // FIXME: Set rendering suppression for view transitions to false.

    if (document()->activeViewTransition() == this)
        clearViewTransition();

    m_phase = ViewTransitionPhase::Done;

    if (reason.hasException())
        m_ready.second->reject(reason.releaseException());
    else {
        m_ready.second->rejectWithCallback([&] (auto&) {
            return reason.releaseReturnValue();
        }, RejectAsHandled::Yes);
    }

    m_updateCallbackDone.first->whenSettled([this, protectedThis = Ref { *this }] {
        switch (m_updateCallbackDone.first->status()) {
        case DOMPromise::Status::Fulfilled:
            m_finished.second->resolve();
            break;
        case DOMPromise::Status::Rejected:
            m_finished.second->rejectWithCallback([&] (auto&) {
                return m_updateCallbackDone.first->result();
            }, RejectAsHandled::Yes);
            break;
        case DOMPromise::Status::Pending:
            ASSERT_NOT_REACHED();
            break;
        }
    });
}

// https://drafts.csswg.org/css-view-transitions/#ViewTransition-skipTransition
void ViewTransition::skipTransition()
{
    if (m_phase != ViewTransitionPhase::Done)
        skipViewTransition(Exception { ExceptionCode::AbortError, "Skipping view transition because skipTransition() was called."_s });
}

// https://drafts.csswg.org/css-view-transitions/#call-dom-update-callback-algorithm
void ViewTransition::callUpdateCallback()
{
    if (!document())
        return;

    ASSERT(m_phase < ViewTransitionPhase::UpdateCallbackCalled || m_phase == ViewTransitionPhase::Done);

    Ref document = *this->document();
    RefPtr<DOMPromise> callbackPromise;
    if (!m_updateCallback) {
        auto promiseAndWrapper = createPromiseAndWrapper(document);
        promiseAndWrapper.second->resolve();
        callbackPromise = WTFMove(promiseAndWrapper.first);
    } else {
        auto result = m_updateCallback->handleEvent();
        callbackPromise = result.type() == CallbackResultType::Success ? result.releaseReturnValue() : nullptr;
        if (!callbackPromise || callbackPromise->isSuspended()) {
            auto promiseAndWrapper = createPromiseAndWrapper(document);
            // FIXME: First case should reject with `ExceptionCode::ExistingExceptionError`.
            if (result.type() == CallbackResultType::ExceptionThrown)
                promiseAndWrapper.second->reject(ExceptionCode::TypeError);
            else
                promiseAndWrapper.second->reject();
            callbackPromise = WTFMove(promiseAndWrapper.first);
        }
    }

    if (m_phase != ViewTransitionPhase::Done)
        m_phase = ViewTransitionPhase::UpdateCallbackCalled;

    callbackPromise->whenSettled([this, weakThis = WeakPtr { *this }, callbackPromise] () mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        switch (callbackPromise->status()) {
        case DOMPromise::Status::Fulfilled:
            m_updateCallbackDone.second->resolve();
            break;
        case DOMPromise::Status::Rejected:
            m_updateCallbackDone.second->rejectWithCallback([&] (auto&) {
                return callbackPromise->result();
            }, RejectAsHandled::No);
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    });
}

// https://drafts.csswg.org/css-view-transitions/#setup-view-transition-algorithm
void ViewTransition::setupViewTransition()
{
    if (!document())
        return;

    ASSERT(m_phase == ViewTransitionPhase::PendingCapture);

    m_phase = ViewTransitionPhase::CapturingOldState;

    auto checkFailure = captureOldState();
    if (checkFailure.hasException()) {
        skipViewTransition(checkFailure.releaseException());
        return;
    }

    // FIXME: Set document’s rendering suppression for view transitions to true.
    protectedDocument()->checkedEventLoop()->queueTask(TaskSource::DOMManipulation, [this, weakThis = WeakPtr { *this }] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (m_phase == ViewTransitionPhase::Done)
            return;

        callUpdateCallback();
        m_updateCallbackDone.first->whenSettled([this, weakThis = WeakPtr { *this }] {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            m_updateCallbackTimeout = nullptr;
            switch (m_updateCallbackDone.first->status()) {
            case DOMPromise::Status::Fulfilled:
                activateViewTransition();
                break;
            case DOMPromise::Status::Rejected:
                if (m_phase == ViewTransitionPhase::Done)
                    return;
                skipViewTransition(m_updateCallbackDone.first->result());
                break;
            case DOMPromise::Status::Pending:
                ASSERT_NOT_REACHED();
                break;
            }
        });

        m_updateCallbackTimeout = protectedDocument()->checkedEventLoop()->scheduleTask(4_s, TaskSource::DOMManipulation, [this, weakThis = WeakPtr { *this }] {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            if (m_phase == ViewTransitionPhase::Done)
                return;
            skipViewTransition(Exception { ExceptionCode::TimeoutError, "View transition update callback timed out."_s });
        });
    });
}

static AtomString effectiveViewTransitionName(RenderLayerModelObject& renderer)
{
    if (renderer.isSkippedContent())
        return nullAtom();
    auto transitionName = renderer.style().viewTransitionName();
    if (!transitionName)
        return nullAtom();
    return transitionName->name;
}

static ExceptionOr<void> checkDuplicateViewTransitionName(const AtomString& name, ListHashSet<AtomString>& usedTransitionNames)
{
    if (usedTransitionNames.contains(name))
        return Exception { ExceptionCode::InvalidStateError, makeString("Multiple elements found with view-transition-name: "_s, name) };
    usedTransitionNames.add(name);
    return { };
}

static LayoutRect captureOverflowRect(RenderLayerModelObject& renderer)
{
    if (!renderer.hasLayer())
        return { };

    if (renderer.isDocumentElementRenderer()) {
        auto& frameView = renderer.view().frameView();
        return { { }, LayoutSize { frameView.frameRect().width(), frameView.frameRect().height() } };
    }

    return renderer.layer()->calculateLayerBounds(renderer.layer(), LayoutSize(), { RenderLayer::IncludeFilterOutsets, RenderLayer::ExcludeHiddenDescendants, RenderLayer::IncludeCompositedDescendants, RenderLayer::PreserveAncestorFlags });
}

static RefPtr<ImageBuffer> snapshotElementVisualOverflowClippedToViewport(LocalFrame& frame, RenderLayerModelObject& renderer, const LayoutRect& snapshotRect)
{
    ASSERT(renderer.hasLayer());
    CheckedRef layerRenderer = renderer;
    if (layerRenderer->isDocumentElementRenderer())
        layerRenderer = layerRenderer->view();

    IntRect paintRect = snappedIntRect(snapshotRect);

    ASSERT(frame.page());
    float scaleFactor = frame.page()->deviceScaleFactor();
    if (frame.page()->delegatesScaling())
        scaleFactor *= frame.page()->pageScaleFactor();

    ASSERT(frame.document());
    auto hostWindow = (frame.document()->view() && frame.document()->view()->root()) ? frame.document()->view()->root()->hostWindow() : nullptr;

    auto buffer = ImageBuffer::create(paintRect.size(), RenderingPurpose::Snapshot, scaleFactor, DestinationColorSpace::SRGB(), PixelFormat::BGRA8, { ImageBufferOptions::Accelerated }, hostWindow);
    if (!buffer)
        return nullptr;

    buffer->context().translate(-paintRect.location());

    auto paintFlags = RenderLayer::paintLayerPaintingCompositingAllPhasesFlags();
    paintFlags.add(RenderLayer::PaintLayerFlag::TemporaryClipRects);
    paintFlags.add(RenderLayer::PaintLayerFlag::AppliedTransform);
    paintFlags.add(RenderLayer::PaintLayerFlag::PaintingSkipDescendantViewTransition);
    layerRenderer->layer()->paint(buffer->context(), paintRect, LayoutSize(), { PaintBehavior::FlattenCompositingLayers, PaintBehavior::Snapshotting }, nullptr, paintFlags);

    return buffer;
}

// This only iterates through elements with a RenderLayer, which is sufficient for View Transitions which force their creation.
static ExceptionOr<void> forEachRendererInPaintOrder(const std::function<ExceptionOr<void>(RenderLayerModelObject&)>& function, RenderLayer& layer)
{
    auto result = function(layer.renderer());
    if (result.hasException())
        return result.releaseException();

    layer.updateLayerListsIfNeeded();

    for (auto* child : layer.negativeZOrderLayers()) {
        auto result = forEachRendererInPaintOrder(function, *child);
        if (result.hasException())
            return result.releaseException();
    }

    for (auto* child : layer.normalFlowLayers()) {
        auto result = forEachRendererInPaintOrder(function, *child);
        if (result.hasException())
            return result.releaseException();
    }

    for (auto* child : layer.positiveZOrderLayers()) {
        auto result = forEachRendererInPaintOrder(function, *child);
        if (result.hasException())
            return result.releaseException();
    }
    return { };
};

// https://drafts.csswg.org/css-view-transitions/#capture-old-state-algorithm
ExceptionOr<void> ViewTransition::captureOldState()
{
    if (!document())
        return { };
    ListHashSet<AtomString> usedTransitionNames;
    Vector<CheckedRef<RenderLayerModelObject>> captureRenderers;

    // Ensure style & render tree are up-to-date.
    protectedDocument()->updateStyleIfNeeded();

    if (CheckedPtr view = document()->renderView()) {
        m_initialLargeViewportSize = view->sizeForCSSLargeViewportUnits();

        auto result = forEachRendererInPaintOrder([&](RenderLayerModelObject& renderer) -> ExceptionOr<void> {
            if (!Styleable::fromRenderer(renderer))
                return { };

            if (auto name = effectiveViewTransitionName(renderer); !name.isNull()) {
                if (auto check = checkDuplicateViewTransitionName(name, usedTransitionNames); check.hasException())
                    return check.releaseException();

                // FIXME: Skip fragmented content.

                renderer.setCapturedInViewTransition(true);
                captureRenderers.append(renderer);
            }
            return { };
        }, *view->layer());
        if (result.hasException())
            return result.releaseException();
    }

    for (auto& renderer : captureRenderers) {
        CapturedElement capture;

        capture.oldProperties = copyElementBaseProperties(renderer.get(), capture.oldSize);
        capture.oldOverflowRect = captureOverflowRect(renderer.get());
        if (RefPtr frame = document()->frame())
            capture.oldImage = snapshotElementVisualOverflowClippedToViewport(*frame, renderer.get(), capture.oldOverflowRect);

        auto transitionName = renderer->style().viewTransitionName();
        m_namedElements.add(transitionName->name, capture);
    }

    for (auto& renderer : captureRenderers) {
        renderer->setCapturedInViewTransition(false);
    }

    return { };
}

// https://drafts.csswg.org/css-view-transitions/#capture-new-state-algorithm
ExceptionOr<void> ViewTransition::captureNewState()
{
    if (!document())
        return { };
    ListHashSet<AtomString> usedTransitionNames;
    if (CheckedPtr view = document()->renderView()) {
        auto result = forEachRendererInPaintOrder([&](RenderLayerModelObject& renderer) -> ExceptionOr<void> {
            std::optional<const Styleable> styleable = Styleable::fromRenderer(renderer);
            if (!styleable)
                return { };

            if (auto name = effectiveViewTransitionName(renderer); !name.isNull()) {
                if (auto check = checkDuplicateViewTransitionName(name, usedTransitionNames); check.hasException())
                    return check.releaseException();

                if (!m_namedElements.contains(name)) {
                    CapturedElement capturedElement;
                    m_namedElements.add(name, capturedElement);
                }
                m_namedElements.find(name)->newElement = *styleable;
            }
            return { };
        }, *view->layer());
        if (result.hasException())
            return result.releaseException();
    }
    return { };
}

void ViewTransition::setupDynamicStyleSheet(const AtomString& name, const CapturedElement& capturedElement)
{
    Ref resolver = protectedDocument()->styleScope().resolver();

    // image animation name rule
    {
        CSSValueListBuilder list;
        list.append(CSSPrimitiveValue::create("-ua-view-transition-fade-out"_s));
        if (capturedElement.newElement)
            list.append(CSSPrimitiveValue::create("-ua-mix-blend-mode-plus-lighter"_s));
        Ref valueList = CSSValueList::createCommaSeparated(WTFMove(list));
        Ref props = MutableStyleProperties::create();
        props->setProperty(CSSPropertyAnimationName, WTFMove(valueList));

        resolver->setViewTransitionStyles(CSSSelector::PseudoElement::ViewTransitionOld, name, props);
    }

    {
        CSSValueListBuilder list;
        list.append(CSSPrimitiveValue::create("-ua-view-transition-fade-in"_s));
        if (capturedElement.oldImage)
            list.append(CSSPrimitiveValue::create("-ua-mix-blend-mode-plus-lighter"_s));
        Ref valueList = CSSValueList::createCommaSeparated(WTFMove(list));
        Ref props = MutableStyleProperties::create();
        props->setProperty(CSSPropertyAnimationName, WTFMove(valueList));

        resolver->setViewTransitionStyles(CSSSelector::PseudoElement::ViewTransitionNew, name, props);
    }

    if (!capturedElement.oldImage || !capturedElement.newElement)
        return;

    // group animation name rule
    {
        Ref list = CSSValueList::createCommaSeparated(CSSPrimitiveValue::create(makeString("-ua-view-transition-group-anim-"_s, name)));
        Ref props = MutableStyleProperties::create();
        props->setProperty(CSSPropertyAnimationName, WTFMove(list));

        resolver->setViewTransitionStyles(CSSSelector::PseudoElement::ViewTransitionGroup, name, props);
    }

    // image pair isolation rule
    {
        Ref props = MutableStyleProperties::create();
        props->setProperty(CSSPropertyIsolation, CSSPrimitiveValue::create(CSSValueID::CSSValueIsolate));

        resolver->setViewTransitionStyles(CSSSelector::PseudoElement::ViewTransitionImagePair, name, props);
    }

    if (!capturedElement.oldProperties)
        return;

    // group keyframes
    Ref props = MutableStyleProperties::createEmpty();
    props->setProperty(CSSPropertyWidth, capturedElement.oldProperties->getPropertyCSSValue(CSSPropertyWidth));
    props->setProperty(CSSPropertyHeight, capturedElement.oldProperties->getPropertyCSSValue(CSSPropertyHeight));
    props->setProperty(CSSPropertyTransform, capturedElement.oldProperties->getPropertyCSSValue(CSSPropertyTransform));
    props->setProperty(CSSPropertyBackdropFilter, capturedElement.oldProperties->getPropertyCSSValue(CSSPropertyBackdropFilter));

    Ref keyframe = StyleRuleKeyframe::create(WTFMove(props));
    keyframe->setKeyText("from"_s);

    Ref keyframes = StyleRuleKeyframes::create(AtomString(makeString("-ua-view-transition-group-anim-"_s, name)));
    keyframes->wrapperAppendKeyframe(WTFMove(keyframe));

    // We can add this to the normal namespace, since we recreate the resolver when the view-transition ends.
    resolver->addKeyframeStyle(WTFMove(keyframes));
}

// https://drafts.csswg.org/css-view-transitions/#setup-transition-pseudo-elements
void ViewTransition::setupTransitionPseudoElements()
{
    protectedDocument()->setHasViewTransitionPseudoElementTree(true);

    for (auto& [name, capturedElement] : m_namedElements.map())
        setupDynamicStyleSheet(name, capturedElement);

    if (RefPtr documentElement = document()->documentElement())
        documentElement->invalidateStyleInternal();
}

// https://drafts.csswg.org/css-view-transitions/#activate-view-transition
void ViewTransition::activateViewTransition()
{
    if (m_phase == ViewTransitionPhase::Done)
        return;

    // Ensure style & render tree are up-to-date.
    protectedDocument()->updateStyleIfNeeded();

    // FIXME: Set rendering suppression for view transitions to false.
    if (!protectedDocument()->renderView() || protectedDocument()->renderView()->sizeForCSSLargeViewportUnits() != m_initialLargeViewportSize) {
        skipViewTransition(Exception { ExceptionCode::InvalidStateError, "Skipping view transition because viewport size changed."_s });
        return;
    }

    auto checkFailure = captureNewState();
    if (checkFailure.hasException()) {
        skipViewTransition(checkFailure.releaseException());
        return;
    }

    for (auto& [name, capturedElement] : m_namedElements.map()) {
        if (auto newStyleable = capturedElement->newElement.styleable()) {
            if (CheckedPtr renderer = newStyleable->renderer())
                renderer->setCapturedInViewTransition(true);
        }
    }

    setupTransitionPseudoElements();
    checkFailure = updatePseudoElementStyles();
    if (checkFailure.hasException()) {
        skipViewTransition(checkFailure.releaseException());
        return;
    }

    m_phase = ViewTransitionPhase::Animating;
    m_ready.second->resolve();
}

// https://drafts.csswg.org/css-view-transitions/#handle-transition-frame-algorithm
void ViewTransition::handleTransitionFrame()
{
    if (!document())
        return;

    RefPtr documentElement = document()->documentElement();
    if (!documentElement)
        return;

    auto checkForActiveAnimations = [&](const Style::PseudoElementIdentifier& pseudoElementIdentifier) {
        if (!documentElement->animations(pseudoElementIdentifier))
            return false;

        for (auto& animation : *documentElement->animations(pseudoElementIdentifier)) {
            auto playState = animation->playState();
            if (playState == WebAnimation::PlayState::Paused || playState == WebAnimation::PlayState::Running)
                return true;
            if (document()->timeline().hasPendingAnimationEventForAnimation(animation))
                return true;
        }
        return false;
    };

    bool hasActiveAnimations = checkForActiveAnimations({ PseudoId::ViewTransition });

    for (auto& name : namedElements().keys()) {
        if (hasActiveAnimations)
            break;
        hasActiveAnimations = checkForActiveAnimations({ PseudoId::ViewTransitionGroup, name })
            || checkForActiveAnimations({ PseudoId::ViewTransitionImagePair, name })
            || checkForActiveAnimations({ PseudoId::ViewTransitionNew, name })
            || checkForActiveAnimations({ PseudoId::ViewTransitionOld, name });
    }

    if (!hasActiveAnimations) {
        m_phase = ViewTransitionPhase::Done;
        clearViewTransition();
        m_finished.second->resolve();
        return;
    }

    if (!protectedDocument()->renderView() || protectedDocument()->renderView()->sizeForCSSLargeViewportUnits() != m_initialLargeViewportSize) {
        skipViewTransition(Exception { ExceptionCode::InvalidStateError, "Skipping view transition because viewport size changed."_s });
        return;
    }

    updatePseudoElementStyles();
}

// https://drafts.csswg.org/css-view-transitions/#clear-view-transition-algorithm
void ViewTransition::clearViewTransition()
{
    if (!document())
        return;

    Ref document = *this->document();
    ASSERT(document->activeViewTransition() == this);

    // End animations on pseudo-elements so they can run again.
    if (RefPtr documentElement = document->documentElement()) {
        Styleable(*documentElement, Style::PseudoElementIdentifier { PseudoId::ViewTransition }).cancelStyleOriginatedAnimations();
        for (auto& name : namedElements().keys()) {
            Styleable(*documentElement, Style::PseudoElementIdentifier { PseudoId::ViewTransitionGroup, name }).cancelStyleOriginatedAnimations();
            Styleable(*documentElement, Style::PseudoElementIdentifier { PseudoId::ViewTransitionImagePair, name }).cancelStyleOriginatedAnimations();
            Styleable(*documentElement, Style::PseudoElementIdentifier { PseudoId::ViewTransitionNew, name }).cancelStyleOriginatedAnimations();
            Styleable(*documentElement, Style::PseudoElementIdentifier { PseudoId::ViewTransitionOld, name }).cancelStyleOriginatedAnimations();
        }
    }

    for (auto& [name, capturedElement] : m_namedElements.map()) {
        if (auto newStyleable = capturedElement->newElement.styleable()) {
            if (CheckedPtr renderer = newStyleable->renderer())
                renderer->setCapturedInViewTransition(false);
        }
    }

    document->setHasViewTransitionPseudoElementTree(false);
    document->styleScope().clearViewTransitionStyles();
    document->setActiveViewTransition(nullptr);

    if (RefPtr documentElement = document->documentElement())
        documentElement->invalidateStyleInternal();
}

Ref<MutableStyleProperties> ViewTransition::copyElementBaseProperties(RenderLayerModelObject& renderer, LayoutSize& size)
{
    std::optional<const Styleable> styleable = Styleable::fromRenderer(renderer);
    ASSERT(styleable);
    ComputedStyleExtractor styleExtractor(&styleable->element, false, styleable->pseudoElementIdentifier);

    CSSPropertyID transitionProperties[] = {
        CSSPropertyWritingMode,
        CSSPropertyDirection,
        CSSPropertyTextOrientation,
        CSSPropertyMixBlendMode,
        CSSPropertyBackdropFilter,
#if ENABLE(DARK_MODE_CSS)
        CSSPropertyColorScheme,
#endif
    };

    Ref<MutableStyleProperties> props = styleExtractor.copyProperties(transitionProperties);

    if (renderer.isDocumentElementRenderer()) {
        auto& frameView = renderer.view().frameView();
        size.setWidth(frameView.frameRect().width());
        size.setHeight(frameView.frameRect().height());
    } else if (CheckedPtr renderBox = dynamicDowncast<RenderBoxModelObject>(&renderer))
        size = renderBox->borderBoundingBox().size();

    props->setProperty(CSSPropertyWidth, CSSPrimitiveValue::create(size.width(), CSSUnitType::CSS_PX));
    props->setProperty(CSSPropertyHeight, CSSPrimitiveValue::create(size.height(), CSSUnitType::CSS_PX));

    TransformationMatrix transform;
    RenderElement* current = &renderer;
    RenderElement* container = nullptr;
    while (current && !current->isRenderView()) {
        container = current->container();
        if (!container)
            break;
        LayoutSize containerOffset = current->offsetFromContainer(*container, LayoutPoint());
        if (container->isRenderView()) {
            auto frameView = current->view().protectedFrameView();
            containerOffset -= toLayoutSize(frameView->scrollPositionRespectingCustomFixedPosition());
        }
        TransformationMatrix localTransform;
        current->getTransformFromContainer(containerOffset, localTransform);
        transform = localTransform * transform;
        current = container;
    }
    // Apply the inverse of what will be added by the default value of 'transform-origin',
    // since the computed transform has already included it.
    transform.translate(size.width() / 2, size.height() / 2);
    transform.translateRight(-size.width() / 2, -size.height() / 2);

    Ref<CSSValue> transformListValue = CSSTransformListValue::create(ComputedStyleExtractor::matrixTransformValue(transform, renderer.style()));
    props->setProperty(CSSPropertyTransform, WTFMove(transformListValue));
    return props;
}

// https://drafts.csswg.org/css-view-transitions-1/#update-pseudo-element-styles
ExceptionOr<void> ViewTransition::updatePseudoElementStyles()
{
    Ref resolver = protectedDocument()->styleScope().resolver();

    for (auto& [name, capturedElement] : m_namedElements.map()) {
        RefPtr<MutableStyleProperties> properties;
        if (auto newStyleable = capturedElement->newElement.styleable()) {
            // FIXME: Also check fragmented content here.
            CheckedPtr renderer = dynamicDowncast<RenderBoxModelObject>(newStyleable->renderer());
            if (!renderer || renderer->isSkippedContent())
                return Exception { ExceptionCode::InvalidStateError, "One of the transitioned elements has become hidden."_s };

            LayoutSize boxSize;
            properties = copyElementBaseProperties(*renderer, boxSize);
            LayoutRect overflowRect = captureOverflowRect(*renderer);

            if (RefPtr documentElement = document()->documentElement()) {
                Styleable styleable(*documentElement, Style::PseudoElementIdentifier { PseudoId::ViewTransitionNew, name });
                if (CheckedPtr viewTransitionCapture = dynamicDowncast<RenderViewTransitionCapture>(styleable.renderer())) {
                    viewTransitionCapture->setSize(boxSize, overflowRect);

                    RefPtr<ImageBuffer> image;
                    if (RefPtr frame = document()->frame(); !viewTransitionCapture->canUseExistingLayers())
                        image = snapshotElementVisualOverflowClippedToViewport(*frame, *renderer, overflowRect);
                    viewTransitionCapture->setImage(image);
                }
            }
        } else
            properties = capturedElement->oldProperties;

        if (properties) {
            // group styles rule
            if (!capturedElement->groupStyleProperties) {
                capturedElement->groupStyleProperties = properties;
                resolver->setViewTransitionStyles(CSSSelector::PseudoElement::ViewTransitionGroup, name, *properties);
            } else
                capturedElement->groupStyleProperties->mergeAndOverrideOnConflict(*properties);
        }
    }

    protectedDocument()->styleScope().didChangeStyleSheetContents();
    return { };
}

RenderViewTransitionCapture* ViewTransition::viewTransitionNewPseudoForCapturedElement(RenderLayerModelObject& renderer)
{
    for (auto& [name, capturedElement] : m_namedElements.map()) {
        if (auto newStyleable = capturedElement->newElement.styleable()) {
            if (newStyleable->renderer() == &renderer) {
                Styleable styleable(*renderer.document().documentElement(), Style::PseudoElementIdentifier { PseudoId::ViewTransitionNew, name });
                return dynamicDowncast<RenderViewTransitionCapture>(styleable.renderer());
            }
        }
    }
    return nullptr;
}

void ViewTransition::stop()
{
    if (!document())
        return;

    m_phase = ViewTransitionPhase::Done;

    if (document()->activeViewTransition() == this)
        clearViewTransition();
}

bool ViewTransition::documentElementIsCaptured() const
{
    RefPtr doc = document();
    if (!doc)
        return false;

    RefPtr documentElement = doc->documentElement();
    if (!documentElement)
        return false;

    CheckedPtr renderer = documentElement->renderer();
    if (!renderer)
        return false;

    return renderer->capturedInViewTransition();
}

}
