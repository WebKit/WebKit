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

#include "CSSFunctionValue.h"
#include "CSSKeyframeRule.h"
#include "CSSKeyframesRule.h"
#include "CSSTransformListValue.h"
#include "CSSValuePool.h"
#include "CheckVisibilityOptions.h"
#include "ContainerNodeInlines.h"
#include "ContextDestructionObserverInlines.h"
#include "DocumentEventLoop.h"
#include "DocumentTimeline.h"
#include "DocumentView.h"
#include "ElementInlines.h"
#include "FrameSnapshotting.h"
#include "HostWindow.h"
#include "JSDOMPromise.h"
#include "JSDOMPromiseDeferred.h"
#include "LayoutRect.h"
#include "Logging.h"
#include "RenderElementInlines.h"
#include "PseudoElementRequest.h"
#include "RenderBoxInlines.h"
#include "RenderFragmentedFlow.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderLayerModelObject.h"
#include "RenderObjectInlines.h"
#include "RenderStyle+GettersInlines.h"
#include "RenderView.h"
#include "RenderViewTransitionCapture.h"
#include "StyleExtractor.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include "StyleTransformFunction.h"
#include "Styleable.h"
#include "TransformState.h"
#include "ViewTransitionTypeSet.h"
#include "WebAnimation.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CapturedElement);
WTF_MAKE_TZONE_ALLOCATED_IMPL(ViewTransitionParams);
WTF_MAKE_TZONE_ALLOCATED_IMPL(ViewTransition);

ViewTransition::ViewTransition(Document& document, RefPtr<ViewTransitionUpdateCallback>&& updateCallback, Vector<AtomString>&& initialActiveTypes)
    : ActiveDOMObject(document)
    , m_updateCallback(WTF::move(updateCallback))
    , m_ready(createPromiseAndWrapper(document))
    , m_updateCallbackDone(createPromiseAndWrapper(document))
    , m_finished(createPromiseAndWrapper(document))
    , m_types(ViewTransitionTypeSet::create(document, WTF::move(initialActiveTypes)))
{
    document.registerForVisibilityStateChangedCallbacks(*this);
}

ViewTransition::ViewTransition(Document& document, Vector<AtomString>&& initialActiveTypes)
    : ActiveDOMObject(document)
    , m_isCrossDocument(true)
    , m_ready(createPromiseAndWrapper(document))
    , m_updateCallbackDone(createPromiseAndWrapper(document))
    , m_finished(createPromiseAndWrapper(document))
    , m_types(ViewTransitionTypeSet::create(document, WTF::move(initialActiveTypes)))
{
}


ViewTransition::~ViewTransition() = default;

Ref<ViewTransition> ViewTransition::createSamePage(Document& document, RefPtr<ViewTransitionUpdateCallback>&& updateCallback, Vector<AtomString>&& initialActiveTypes)
{
    Ref viewTransition = adoptRef(*new ViewTransition(document, WTF::move(updateCallback), WTF::move(initialActiveTypes)));

    LOG_WITH_STREAM(ViewTransitions, stream << "ViewTransition::createSamePage created transition " << viewTransition.ptr());

    viewTransition->suspendIfNeeded();
    return viewTransition;
}

// https://www.w3.org/TR/css-view-transitions-2/#resolve-inbound-cross-document-view-transition
RefPtr<ViewTransition> ViewTransition::resolveInboundCrossDocumentViewTransition(Document& document, std::unique_ptr<ViewTransitionParams> inboundViewTransitionParams)
{
    if (!inboundViewTransitionParams)
        return nullptr;

    if (MonotonicTime::now() - inboundViewTransitionParams->startTime > defaultTimeout)
        return nullptr;

    if (document.activeViewTransition())
        return nullptr;

    auto types = document.resolveViewTransitionRule();
    if (std::holds_alternative<Document::SkipTransition>(types))
        return nullptr;

    RefPtr viewTransition = adoptRef(*new ViewTransition(document, WTF::move(std::get<Vector<AtomString>>(types))));
    viewTransition->suspendIfNeeded();

    viewTransition->m_namedElements.swap(inboundViewTransitionParams->namedElements);
    viewTransition->m_initialLargeViewportSize = inboundViewTransitionParams->initialLargeViewportSize;
    viewTransition->m_initialPageZoom = inboundViewTransitionParams->initialPageZoom;

    document.setActiveViewTransition(protect(viewTransition));

    protect(viewTransition->m_updateCallbackDone.second)->resolve();
    viewTransition->m_phase = ViewTransitionPhase::UpdateCallbackCalled;

    return viewTransition;
}

// https://drafts.csswg.org/css-view-transitions-2/#setup-cross-document-view-transition
Ref<ViewTransition> ViewTransition::setupCrossDocumentViewTransition(Document& document)
{
    auto types = document.resolveViewTransitionRule();
    ASSERT(!std::holds_alternative<Document::SkipTransition>(types));

    if (RefPtr activeViewTransition =  document.activeViewTransition())
        activeViewTransition->skipViewTransition(Exception { ExceptionCode::AbortError, "Old view transition aborted by new view transition."_s });

    Ref viewTransition = adoptRef(*new ViewTransition(document, WTF::move(std::get<Vector<AtomString>>(types))));
    viewTransition->suspendIfNeeded();

    document.setActiveViewTransition(protect(viewTransition.ptr()));

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

    LOG_WITH_STREAM(ViewTransitions, stream << "ViewTransition " << this << " skipViewTransition - phase " << m_phase);

    ASSERT(m_phase != ViewTransitionPhase::Done);

    Ref document = *this->document();
    if (m_phase < ViewTransitionPhase::UpdateCallbackCalled) {
        document->checkedEventLoop()->queueTask(TaskSource::DOMManipulation, [weakThis = WeakPtr { *this }] {
            RefPtr protectedThis = weakThis.get();
            if (protectedThis && protect(protectedThis->document())->globalObject())
                protectedThis->callUpdateCallback();
        });

        if (m_isCrossDocument)
            protect(m_updateCallbackDone.second)->resolve();
    }

    document->clearRenderingIsSuppressedForViewTransition();

    if (document->activeViewTransition() == this)
        clearViewTransition();

    m_phase = ViewTransitionPhase::Done;

    if (reason.hasException())
        protect(m_ready.second)->reject(reason.releaseException());
    else {
        protect(m_ready.second)->rejectWithCallback([&] (auto&) {
            return reason.releaseReturnValue();
        }, RejectAsHandled::Yes);
    }

    protect(m_updateCallbackDone.first)->whenSettled([this, protectedThis = Ref { *this }] {
        if (isContextStopped())
            return;

        switch (protect(m_updateCallbackDone.first)->status()) {
        case DOMPromise::Status::Fulfilled:
            protect(m_finished.second)->resolve();
            break;
        case DOMPromise::Status::Rejected:
            protect(m_finished.second)->rejectWithCallback([this, protectedThis = Ref { *this }] (auto&) {
                return protect(m_updateCallbackDone.first)->result();
            }, RejectAsHandled::Yes);
            break;
        case DOMPromise::Status::Pending:
            ASSERT_NOT_REACHED();
            break;
        }
    });
}

bool ViewTransition::virtualHasPendingActivity() const
{
    return m_phase != ViewTransitionPhase::Done;
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

    LOG_WITH_STREAM(ViewTransitions, stream << "ViewTransition " << this << " callUpdateCallback");

    ASSERT(m_phase < ViewTransitionPhase::UpdateCallbackCalled || m_phase == ViewTransitionPhase::Done);

    if (m_phase != ViewTransitionPhase::Done)
        m_phase = ViewTransitionPhase::UpdateCallbackCalled;

    if (m_isCrossDocument)
        return;

    Ref document = *this->document();
    Ref callbackPromise = [&] -> Ref<DOMPromise> {
        if (!m_updateCallback) {
            auto promiseAndWrapper = createPromiseAndWrapper(document);
            protect(promiseAndWrapper.second)->resolve();
            return WTF::move(promiseAndWrapper.first);
        }

        auto result = protect(m_updateCallback)->invoke();
        if (result.type() != CallbackResultType::Success || result.returnValue()->isSuspended()) {
            auto promiseAndWrapper = createPromiseAndWrapper(document);
            // FIXME: First case should reject with `ExceptionCode::ExistingExceptionError`.
            if (result.type() == CallbackResultType::ExceptionThrown)
                protect(promiseAndWrapper.second)->reject(ExceptionCode::TypeError);
            else
                protect(promiseAndWrapper.second)->reject();
            return WTF::move(promiseAndWrapper.first);
        }

        return result.releaseReturnValue();
    }();

    callbackPromise->whenSettledWithResult([weakThis = WeakPtr { *this }](auto*, bool isFulfilled, auto result) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        protectedThis->m_updateCallbackTimeout = nullptr;
        if (isFulfilled) {
            protect(protectedThis->m_updateCallbackDone.second)->resolve();
            protectedThis->activateViewTransition();
            return;
        }

        protect(protectedThis->m_updateCallbackDone.second)->rejectWithCallback([&result] (auto&) {
            return result;
        }, RejectAsHandled::No);
        if (protectedThis->m_phase == ViewTransitionPhase::Done)
            return;
        protect(protectedThis->m_ready.second)->markAsHandled();
        protectedThis->skipViewTransition(WTF::move(result));
    });

    m_updateCallbackTimeout = document->checkedEventLoop()->scheduleTask(defaultTimeout, TaskSource::DOMManipulation, [weakThis = WeakPtr { *this }] {
        RefPtr protectedThis = weakThis.get();
        LOG_WITH_STREAM(ViewTransitions, stream << "ViewTransition " << protectedThis.get() << " update callback timed out");
        if (!protectedThis)
            return;
        if (protectedThis->m_phase == ViewTransitionPhase::Done)
            return;
        protectedThis->skipViewTransition(Exception { ExceptionCode::TimeoutError, "View transition update callback timed out."_s });
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

    Ref document = *this->document();
    if (m_isCrossDocument)
        document->setRenderingIsSuppressedForViewTransitionImmediately();
    else
        document->setRenderingIsSuppressedForViewTransitionAfterUpdateRendering();

    document->checkedEventLoop()->queueTask(TaskSource::DOMManipulation, [weakThis = WeakPtr { *this }] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (protectedThis->m_phase == ViewTransitionPhase::Done)
            return;

        protectedThis->callUpdateCallback();
    });
}

static AtomString effectiveViewTransitionName(RenderLayerModelObject& renderer, Element& originatingElement, Style::Scope& documentScope, bool isCrossDocument)
{
    if (renderer.isSkippedContent())
        return nullAtom();

    auto& transitionName = renderer.style().viewTransitionName();

    auto computeScope = [&] -> Style::Scope* {
        SUPPRESS_UNCHECKED_LOCAL auto scope = Style::Scope::forOrdinal(originatingElement, transitionName.scopeOrdinal());
        if (!scope || scope != &documentScope)
            return nullptr;
        return scope;
    };

    return WTF::switchOn(transitionName,
        [&](const CSS::Keyword::None&) {
            return nullAtom();
        },
        [&](const CSS::Keyword::Auto&) {
            SUPPRESS_UNCHECKED_LOCAL auto scope = computeScope();
            if (!scope || !renderer.element())
                return nullAtom();

            Ref element = *renderer.element();
            if (scope == &Style::Scope::forNode(element) && element->hasID())
                return makeAtomString("-ua-id-"_s, renderer.protectedElement()->getIdAttribute());

            if (isCrossDocument)
                return nullAtom();

            return makeAtomString("-ua-auto-"_s, String::number(element->nodeIdentifier().toRawValue()));
        },
        [&](const CSS::Keyword::MatchElement&) {
            SUPPRESS_UNCHECKED_LOCAL auto scope = computeScope();
            if (!scope || isCrossDocument || !renderer.element())
                return nullAtom();

            Ref element = *renderer.element();
            return makeAtomString("-ua-auto-"_s, String::number(element->nodeIdentifier().toRawValue()));
        },
        [&](const CustomIdentifier& customIdentifier) {
            SUPPRESS_UNCHECKED_LOCAL auto scope = computeScope();
            if (!scope)
                return nullAtom();

            return customIdentifier.value;
        }
    );
}

static ExceptionOr<void> checkDuplicateViewTransitionName(const AtomString& name, ListHashSet<AtomString>& usedTransitionNames)
{
    if (usedTransitionNames.contains(name))
        return Exception { ExceptionCode::InvalidStateError, makeString("Multiple elements found with view-transition-name: "_s, name) };
    usedTransitionNames.add(name);
    return { };
}

static Vector<AtomString> effectiveViewTransitionClassList(RenderLayerModelObject& renderer, Element& originatingElement, Style::Scope& documentScope)
{
    return WTF::switchOn(renderer.style().viewTransitionClasses(),
        [](const CSS::Keyword::None&) -> Vector<AtomString> {
            return { };
        },
        [&](const auto& list) -> Vector<AtomString> {
            auto scope = Style::Scope::forOrdinal(originatingElement, list[0].scopeOrdinal);
            if (!scope || scope != &documentScope)
                return { };

            return WTF::map(list, [&](auto& item) {
                return item.name;
            });
        }
    );
}

LayoutRect ViewTransition::captureOverflowRect(RenderLayerModelObject& renderer)
{
    if (!renderer.hasLayer())
        return { };

    if (renderer.isDocumentElementRenderer())
        return containingBlockRect();

    auto bounds = renderer.layer()->calculateLayerBounds(renderer.layer(), LayoutSize(), { RenderLayer::IncludeFilterOutsets, RenderLayer::ExcludeHiddenDescendants, RenderLayer::IncludeCompositedDescendants, RenderLayer::PreserveAncestorFlags, RenderLayer::ExcludeViewTransitionCapturedDescendants });
    return LayoutRect(encloseRectToDevicePixels(bounds, protect(renderer.document())->deviceScaleFactor()));
}

// The computed local-to-absolute transform, and layer bounds don't include the position
// of a RenderInline. Manually add an extra offset to adjust for it.
static LayoutPoint layerToLayoutOffset(const RenderLayerModelObject& renderer)
{
    if (const auto* renderInline = dynamicDowncast<RenderInline>(renderer)) {
        auto boundingBox = renderInline->linesBoundingBox();
        return LayoutPoint { boundingBox.x(), boundingBox.y() };
    }
    return { };
}

static RefPtr<ImageBuffer> snapshotElementVisualOverflowClippedToViewport(LocalFrame& frame, RenderLayerModelObject& renderer, const LayoutRect& snapshotRect, const LayoutSize& subpixelOffset = { })
{
    ASSERT(renderer.hasLayer());
    CheckedRef layerRenderer = renderer;

    IntRect paintRect = enclosingIntRect(snapshotRect);

    if (layerRenderer->isDocumentElementRenderer()) {
        CheckedRef view = layerRenderer->view();
        layerRenderer = view.get();

        auto scrollPosition = protect(view->frameView())->scrollPosition();
        paintRect.moveBy(scrollPosition);
    }

    ASSERT(frame.page());
    float scaleFactor = frame.page()->deviceScaleFactor();

    ASSERT(frame.document());
    RefPtr frameView = frame.document()->view();
    if (!frameView)
        return nullptr;
    auto hostWindow = frameView->root() ? protect(frameView->root())->hostWindow() : nullptr;

    auto buffer = ImageBuffer::create(paintRect.size(), RenderingMode::Accelerated, RenderingPurpose::Snapshot, scaleFactor, DestinationColorSpace::SRGB(), PixelFormat::BGRA8, hostWindow);
    if (!buffer)
        return nullptr;

    buffer->context().translate(-paintRect.location());

    auto oldPaintBehavior = frameView->paintBehavior();
    frameView->setPaintBehavior(oldPaintBehavior | PaintBehavior::FlattenCompositingLayers | PaintBehavior::Snapshotting);

    auto paintFlags = RenderLayer::paintLayerPaintingCompositingAllPhasesFlags();
    paintFlags.add(RenderLayer::PaintLayerFlag::TemporaryClipRects);
    paintFlags.add(RenderLayer::PaintLayerFlag::AppliedTransform);
    paintFlags.add(RenderLayer::PaintLayerFlag::PaintingSkipDescendantViewTransition);
    layerRenderer->layer()->paint(buffer->context(), paintRect, subpixelOffset, frameView->paintBehavior(), nullptr, paintFlags);

    frameView->setPaintBehavior(oldPaintBehavior);
    return buffer;
}

// This only iterates through elements with a RenderLayer, which is sufficient for View Transitions which force their creation.
static ExceptionOr<void> forEachRendererInPaintOrder(NOESCAPE const std::function<ExceptionOr<void>(RenderLayerModelObject&)>& function, RenderLayer& layer)
{
    auto result = function(layer.renderer());
    if (result.hasException())
        return result.releaseException();

    if (auto* renderBox = dynamicDowncast<RenderBox>(layer.renderer()); renderBox && isSkippedContentRoot(*renderBox))
        return { };

    layer.updateLayerListsIfNeeded();

#if ASSERT_ENABLED
    LayerListMutationDetector mutationChecker(layer);
#endif

    for (CheckedPtr child : layer.negativeZOrderLayers()) {
        auto result = forEachRendererInPaintOrder(function, *child);
        if (result.hasException())
            return result.releaseException();
    }

    for (CheckedPtr child : layer.normalFlowLayers()) {
        auto result = forEachRendererInPaintOrder(function, *child);
        if (result.hasException())
            return result.releaseException();
    }

    for (CheckedPtr child : layer.positiveZOrderLayers()) {
        auto result = forEachRendererInPaintOrder(function, *child);
        if (result.hasException())
            return result.releaseException();
    }
    return { };
};

static bool rendererIsFragmented(const RenderLayerModelObject& renderer)
{
    // https://drafts.csswg.org/css-view-transitions-1/#capture-old-state-algorithm
    // View transitions explicitly excludes splitting of inline boxes across lines.

    CheckedPtr box = dynamicDowncast<RenderBox>(renderer);
    if (!box)
        return false;

    CheckedPtr enclosingFragmentedFlow = renderer.enclosingFragmentedFlow();
    if (!enclosingFragmentedFlow)
        return false;

    return enclosingFragmentedFlow->boxIsFragmented(*box);
}

// https://drafts.csswg.org/css-view-transitions/#capture-old-state-algorithm
ExceptionOr<void> ViewTransition::captureOldState()
{
    if (!document())
        return { };
    ListHashSet<AtomString> usedTransitionNames;
    Vector<CheckedRef<RenderLayerModelObject>> captureRenderers;

    // Ensure style & layout are up-to-date.
    protect(document())->updateLayoutIgnorePendingStylesheets();

    if (CheckedPtr view = document()->renderView()) {
        Ref frame = protect(view->frameView())->frame();
        m_initialLargeViewportSize = view->sizeForCSSLargeViewportUnits();
        m_initialPageZoom = frame->pageZoomFactor() * frame->frameScaleFactor();

        auto result = forEachRendererInPaintOrder([&](RenderLayerModelObject& renderer) -> ExceptionOr<void> {
            auto styleable = Styleable::fromRenderer(renderer);
            if (!styleable)
                return { };

            if (rendererIsFragmented(renderer))
                return { };

            if (auto name = effectiveViewTransitionName(renderer, protect(styleable->element), document()->styleScope(), isCrossDocument()); !name.isNull()) {
                if (auto check = checkDuplicateViewTransitionName(name, usedTransitionNames); check.hasException())
                    return check.releaseException();

                renderer.setCapturedInViewTransition(true);
                captureRenderers.append(renderer);
            }
            return { };
        }, *view->layer());
        if (result.hasException()) {
            for (auto& renderer : captureRenderers)
                renderer->setCapturedInViewTransition(false);
            return result.releaseException();
        }
    }

    for (auto& renderer : captureRenderers) {
        CapturedElement capture;

        copyElementBaseProperties(renderer.get(), capture.oldState);
        if (RefPtr frame = document()->frame())
            capture.oldImage = snapshotElementVisualOverflowClippedToViewport(*frame, renderer.get(), capture.oldState.overflowRect, capture.oldState.subpixelOffset);

        auto styleable = Styleable::fromRenderer(renderer);
        ASSERT(styleable);
        Ref element = styleable->element;
        capture.classList = effectiveViewTransitionClassList(renderer, element, document()->styleScope());

        auto transitionName = effectiveViewTransitionName(renderer, element, document()->styleScope(), isCrossDocument());
        m_namedElements.add(transitionName, capture);
    }

    for (auto& [name, capturedElement] : m_namedElements.map()) {
        if (capturedElement->oldState.intersectsViewport && capturedElement->oldImage) {
            if (RefPtr oldImage = *capturedElement->oldImage)
                oldImage->flushDrawingContextAsync();
        }
    }

    for (auto& renderer : captureRenderers)
        renderer->setCapturedInViewTransition(false);

    return { };
}

bool ViewTransition::updatePropertiesForGroupPseudo(CapturedElement& capturedElement, const AtomString& name)
{
    RefPtr properties = capturedElement.newState.properties ? capturedElement.newState.properties : capturedElement.oldState.properties;
    if (properties) {
        // group styles rule
        if (!capturedElement.groupStyleProperties) {
            capturedElement.groupStyleProperties = properties;
            protect(document())->styleScope().protectedResolver()->setViewTransitionStyles(CSSSelector::PseudoElement::ViewTransitionGroup, name, *properties);
            return true;
        }
        return protect(*capturedElement.groupStyleProperties)->mergeAndOverrideOnConflict(*properties);
    }
    return false;
}

// https://drafts.csswg.org/css-view-transitions/#capture-new-state-algorithm
ExceptionOr<void> ViewTransition::captureNewState()
{
    if (!document())
        return { };
    ListHashSet<AtomString> usedTransitionNames;
    if (CheckedPtr view = document()->renderView()) {
        auto result = forEachRendererInPaintOrder([&](RenderLayerModelObject& renderer) -> ExceptionOr<void> {
            auto styleable = Styleable::fromRenderer(renderer);
            if (!styleable)
                return { };

            if (rendererIsFragmented(renderer))
                return { };

            Ref element = styleable->element;
            if (auto name = effectiveViewTransitionName(renderer, element, document()->styleScope(), isCrossDocument()); !name.isNull()) {
                if (auto check = checkDuplicateViewTransitionName(name, usedTransitionNames); check.hasException())
                    return check.releaseException();

                if (!m_namedElements.contains(name)) {
                    CapturedElement capturedElement;
                    m_namedElements.add(name, capturedElement);
                }
                auto namedElement = m_namedElements.find(name);
                namedElement->classList = effectiveViewTransitionClassList(renderer, element, document()->styleScope());
                namedElement->newElement = *styleable;

                // Do the work on updatePseudoElementStylesRead now
                // to avoid needing an extra iteration later on.
                if (CheckedPtr box = dynamicDowncast<RenderBoxModelObject>(renderer))
                    copyElementBaseProperties(*box, namedElement->newState);
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
    Ref resolver = protect(document())->styleScope().resolver();

    // image animation name rule
    if (capturedElement.oldImage) {
        CSSValueListBuilder list;
        list.append(CSSPrimitiveValue::createCustomIdent("-ua-view-transition-fade-out"_s));
        if (capturedElement.newElement)
            list.append(CSSPrimitiveValue::createCustomIdent("-ua-mix-blend-mode-plus-lighter"_s));
        Ref valueList = CSSValueList::createCommaSeparated(WTF::move(list));
        Ref props = MutableStyleProperties::create();
        props->setProperty(CSSPropertyAnimationName, WTF::move(valueList));

        resolver->setViewTransitionStyles(CSSSelector::PseudoElement::ViewTransitionOld, name, props);
    }

    if (capturedElement.newElement) {
        CSSValueListBuilder list;
        list.append(CSSPrimitiveValue::createCustomIdent("-ua-view-transition-fade-in"_s));
        if (capturedElement.oldImage)
            list.append(CSSPrimitiveValue::createCustomIdent("-ua-mix-blend-mode-plus-lighter"_s));
        Ref valueList = CSSValueList::createCommaSeparated(WTF::move(list));
        Ref props = MutableStyleProperties::create();
        props->setProperty(CSSPropertyAnimationName, WTF::move(valueList));

        resolver->setViewTransitionStyles(CSSSelector::PseudoElement::ViewTransitionNew, name, props);
    }

    if (!capturedElement.oldImage || !capturedElement.newElement)
        return;

    // group animation name rule
    {
        Ref list = CSSValueList::createCommaSeparated(CSSPrimitiveValue::createCustomIdent(makeString("-ua-view-transition-group-anim-"_s, name)));
        Ref props = MutableStyleProperties::create();
        props->setProperty(CSSPropertyAnimationName, WTF::move(list));

        resolver->setViewTransitionStyles(CSSSelector::PseudoElement::ViewTransitionGroup, name, props);
    }

    // image pair isolation rule
    {
        Ref props = MutableStyleProperties::create();
        props->setProperty(CSSPropertyIsolation, CSSPrimitiveValue::create(CSSValueID::CSSValueIsolate));

        resolver->setViewTransitionStyles(CSSSelector::PseudoElement::ViewTransitionImagePair, name, props);
    }

    if (!capturedElement.oldState.properties)
        return;

    // group keyframes
    static constexpr auto keyframeProperties = std::to_array<CSSPropertyID>({
        CSSPropertyWidth,
        CSSPropertyHeight,
        CSSPropertyTransform,
        CSSPropertyBackdropFilter,
    });
    Ref keyframe = StyleRuleKeyframe::create(protect(capturedElement.oldState.properties)->copyProperties(keyframeProperties));
    keyframe->setKeyText("from"_s);

    Ref keyframes = StyleRuleKeyframes::create(AtomString(makeString("-ua-view-transition-group-anim-"_s, name)));
    keyframes->wrapperAppendKeyframe(WTF::move(keyframe));

    // We can add this to the normal namespace, since we recreate the resolver when the view-transition ends.
    resolver->addKeyframeStyle(WTF::move(keyframes));
}

// https://drafts.csswg.org/css-view-transitions/#setup-transition-pseudo-elements
void ViewTransition::setupTransitionPseudoElements()
{
    protect(document())->setHasViewTransitionPseudoElementTree(true);

    for (auto& [name, capturedElement] : m_namedElements.map())
        setupDynamicStyleSheet(name, capturedElement);
}

ExceptionOr<void> ViewTransition::checkForViewportSizeChange()
{
    CheckedPtr view = protect(document())->renderView();
    if (!view)
        return Exception { ExceptionCode::InvalidStateError, "Skipping view transition because viewport size changed."_s };

    Ref frame = protect(view->frameView())->frame();
    if (view->sizeForCSSLargeViewportUnits() != m_initialLargeViewportSize || m_initialPageZoom != (frame->pageZoomFactor() * frame->frameScaleFactor()))
        return Exception { ExceptionCode::InvalidStateError, "Skipping view transition because viewport size changed."_s };
    return { };
}

// https://drafts.csswg.org/css-view-transitions/#activate-view-transition
void ViewTransition::activateViewTransition()
{
    if (m_phase == ViewTransitionPhase::Done)
        return;

    protect(document())->clearRenderingIsSuppressedForViewTransition();

    // Ensure style & layout are up-to-date.
    protect(document())->updateLayoutIgnorePendingStylesheets();

    auto checkSize = checkForViewportSizeChange();
    if (checkSize.hasException()) {
        skipViewTransition(checkSize.releaseException());
        return;
    }

    auto checkFailure = captureNewState();
    if (checkFailure.hasException()) {
        skipViewTransition(checkFailure.releaseException());
        return;
    }

    setupTransitionPseudoElements();

    for (auto& [name, capturedElement] : m_namedElements.map()) {
        if (auto newStyleable = capturedElement->newElement.styleable())
            newStyleable->setCapturedInViewTransition(name);
    }

    if (RefPtr documentElement = document()->documentElement())
        documentElement->invalidateStyleInternal();

    m_phase = ViewTransitionPhase::Animating;

    // Don't read pseudo-element styles, since that happened
    // as part of captureNewState.
    updatePseudoElementStylesWrite();
    updatePseudoElementRenderers();

    protect(m_ready.second)->resolve();
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

        Ref timeline = protect(document())->timeline();
        for (auto& animation : *documentElement->animations(pseudoElementIdentifier)) {
            auto playState = animation->playState();
            if (playState == WebAnimation::PlayState::Paused || playState == WebAnimation::PlayState::Running)
                return true;
            if (timeline->hasPendingAnimationEventForAnimation(animation))
                return true;
        }
        return false;
    };

    bool hasActiveAnimations = checkForActiveAnimations({ PseudoElementType::ViewTransition });

    for (auto& name : namedElements().keys()) {
        if (hasActiveAnimations)
            break;
        hasActiveAnimations = checkForActiveAnimations({ PseudoElementType::ViewTransitionGroup, name })
            || checkForActiveAnimations({ PseudoElementType::ViewTransitionImagePair, name })
            || checkForActiveAnimations({ PseudoElementType::ViewTransitionNew, name })
            || checkForActiveAnimations({ PseudoElementType::ViewTransitionOld, name });
    }

    if (!hasActiveAnimations) {
        m_phase = ViewTransitionPhase::Done;
        clearViewTransition();
        protect(m_finished.second)->resolve();
        return;
    }

    auto checkSize = checkForViewportSizeChange();
    if (checkSize.hasException()) {
        skipViewTransition(checkSize.releaseException());
        return;
    }

    auto checkPseudoStyles = updatePseudoElementStylesRead();
    if (checkPseudoStyles.hasException()) {
        skipViewTransition(checkPseudoStyles.releaseException());
        return;
    }

    updatePseudoElementStylesWrite();
    updatePseudoElementRenderers();
}

// https://drafts.csswg.org/css-view-transitions/#clear-view-transition-algorithm
void ViewTransition::clearViewTransition()
{
    if (!document())
        return;

    Ref document = *this->document();
    ASSERT(document->activeViewTransition() == this);

    for (auto& [name, capturedElement] : m_namedElements.map()) {
        if (auto newStyleable = capturedElement->newElement.styleable())
            newStyleable->setCapturedInViewTransition(nullAtom());
    }

    document->setHasViewTransitionPseudoElementTree(false);
    document->styleScope().clearViewTransitionStyles();
    document->setActiveViewTransition(nullptr);

    if (RefPtr documentElement = document->documentElement())
        documentElement->invalidateStyleInternal();
}

// https://drafts.csswg.org/css-view-transitions-1/#snapshot-containing-block
LayoutRect ViewTransition::containingBlockRect()
{
    RefPtr document = this->document();
    if (!document)
        return { };
    RefPtr frameView = document->view();
    if (!frameView)
        return { };
    // FIXME: Bug 285400 - Correctly account for insets.
    return { LayoutPoint { }, frameView->visibleContentRectIncludingScrollbars().size() };
}


// Rounds the x/y translation components to the nearest pixel (for non-perspective)
// transforms, and returns the subpixel offset that was removed.
static LayoutSize snapTransformationTranslationToDevicePixels(TransformationMatrix& matrix, float deviceScaleFactor)
{
    if (matrix.hasPerspective())
        return { };

    LayoutSize oldTranslation(matrix.m41(), matrix.m42());
    matrix.setM41(std::round(matrix.m41() * deviceScaleFactor) / deviceScaleFactor);
    matrix.setM42(std::round(matrix.m42() * deviceScaleFactor) / deviceScaleFactor);
    return oldTranslation - LayoutSize(matrix.m41(), matrix.m42());
}


void ViewTransition::copyElementBaseProperties(RenderLayerModelObject& renderer, CapturedElement::State& output)
{
    std::optional<const Styleable> styleable = Styleable::fromRenderer(renderer);
    ASSERT(styleable);
    Style::Extractor styleExtractor { &styleable->element, false, styleable->pseudoElementIdentifier };

    static constexpr auto transitionProperties = std::to_array<CSSPropertyID>({
        CSSPropertyWritingMode,
        CSSPropertyDirection,
        CSSPropertyTextOrientation,
        CSSPropertyMixBlendMode,
        CSSPropertyBackdropFilter,
#if ENABLE(DARK_MODE_CSS)
        CSSPropertyColorScheme,
#endif
    });

    output.overflowRect = captureOverflowRect(renderer);
    output.properties = styleExtractor.copyProperties(transitionProperties);

    CheckedRef frameView = renderer.view().frameView();

    RefPtr documentElement = renderer.document().documentElement();
    if (!documentElement)
        return;
    CheckedPtr documentElementRenderer = documentElement->renderer();
    if (!documentElementRenderer)
        return;

    if (renderer.isDocumentElementRenderer()) {
        output.size.setWidth(containingBlockRect().width());
        output.size.setHeight(containingBlockRect().height());
        output.isRootElement = true;
    } else if (CheckedPtr renderBox = dynamicDowncast<RenderBoxModelObject>(&renderer)) {
        output.isRootElement = false;
        output.size = renderBox->borderBoundingBox().size();

        auto transformState = renderer.viewTransitionTransform();
        TransformationMatrix transform;
        if (transformState.accumulatedTransform()) {
            transform = *transformState.accumulatedTransform();
            output.subpixelOffset = { };
        } else {
            transform.translate(transformState.accumulatedOffset().width(), transformState.accumulatedOffset().height());
            output.subpixelOffset = snapTransformationTranslationToDevicePixels(transform, protect(renderer.document())->deviceScaleFactor());
        }

        output.layerToLayoutOffset = layerToLayoutOffset(renderer);
        transform.translate(output.layerToLayoutOffset.x(), output.layerToLayoutOffset.y());

        auto offset = -toFloatSize(frameView->visibleContentRect().location());
        transform.translateRight(offset.width(), offset.height());

        auto mapped = transform.mapRect(output.overflowRect);
        output.intersectsViewport = mapped.intersects(frameView->boundsRect());

        // Apply the inverse of what will be added by the default value of 'transform-origin',
        // since the computed transform has already included it.
        transform.translate(output.size.width() / 2, output.size.height() / 2);
        transform.translateRight(-output.size.width() / 2, -output.size.height() / 2);

        Ref transformListValue = CSSTransformListValue::create(Style::createCSSValue(CSSValuePool::singleton(), documentElementRenderer->style(), transform));
        protect(output.properties)->setProperty(CSSPropertyTransform, WTF::move(transformListValue));
    }

    // Factor out the zoom from the nearest common ancestor of the captured element and the view transition
    // pseudo tree (the document element), so that it doesn't get applied a second time when rendering the
    // snapshots.
    LayoutSize cssSize = adjustLayoutSizeForAbsoluteZoom(output.size, documentElementRenderer->style());
    protect(output.properties)->setProperty(CSSPropertyWidth, CSSPrimitiveValue::create(cssSize.width(), CSSUnitType::CSS_PX));
    protect(output.properties)->setProperty(CSSPropertyHeight, CSSPrimitiveValue::create(cssSize.height(), CSSUnitType::CSS_PX));
}

// https://drafts.csswg.org/css-view-transitions-1/#update-pseudo-element-styles
// Perform all reads required without making any mutations
ExceptionOr<void> ViewTransition::updatePseudoElementStylesRead()
{
    RefPtr document = this->document();
    if (!document)
        return { };

    document->updateLayoutIgnorePendingStylesheets();

    for (auto& [name, capturedElement] : m_namedElements.map()) {
        if (auto newStyleable = capturedElement->newElement.styleable()) {
            CheckedPtr renderer = dynamicDowncast<RenderBoxModelObject>(newStyleable->renderer());
            if (!renderer || rendererIsFragmented(*renderer))
                return Exception { ExceptionCode::InvalidStateError, "One of the transitioned elements is longer renderer or is fragmented."_s };

            copyElementBaseProperties(*renderer, capturedElement->newState);
        }
    }
    return { };
}

// https://drafts.csswg.org/css-view-transitions-1/#update-pseudo-element-styles
// Writes all the new styles using the data from the read pass.
void ViewTransition::updatePseudoElementStylesWrite()
{
    RefPtr document = this->document();
    if (!document)
        return;

    bool changed = false;
    for (auto& [name, capturedElement] : m_namedElements.map())
        changed |= updatePropertiesForGroupPseudo(capturedElement, name);

    if (changed) {
        if (RefPtr documentElement = document->documentElement())
            documentElement->invalidateStyleInternal();
    }
}

ExceptionOr<void> ViewTransition::updatePseudoElementRenderers()
{
    RefPtr document = this->document();
    if (!document)
        return { };

    RefPtr documentElement = document->documentElement();
    if (!documentElement)
        return { };

    document->updateStyleIfNeededIgnoringPendingStylesheets();

    for (auto& [name, capturedElement] : m_namedElements.map()) {
        if (auto newStyleable = capturedElement->newElement.styleable()) {
            // FIXME: Also check fragmented content here.
            CheckedPtr renderer = dynamicDowncast<RenderBoxModelObject>(newStyleable->renderer());
            if (!renderer || renderer->isSkippedContent())
                return Exception { ExceptionCode::InvalidStateError, "One of the transitioned elements has become hidden."_s };

            Styleable styleable(*documentElement, Style::PseudoElementIdentifier { PseudoElementType::ViewTransitionNew, name });
            if (CheckedPtr viewTransitionCapture = dynamicDowncast<RenderViewTransitionCapture>(styleable.renderer())) {
                if (viewTransitionCapture->setCapturedSize(capturedElement->newState.size, capturedElement->newState.overflowRect, capturedElement->newState.layerToLayoutOffset))
                    viewTransitionCapture->setNeedsLayout();

                RefPtr<ImageBuffer> image;
                if (RefPtr frame = document->frame(); !viewTransitionCapture->canUseExistingLayers()) {
                    document->updateLayoutIgnorePendingStylesheets();
                    image = snapshotElementVisualOverflowClippedToViewport(*frame, *renderer, capturedElement->newState.overflowRect);
                } else if (CheckedPtr layer = renderer->isDocumentElementRenderer() ? renderer->view().layer() : renderer->layer())
                    layer->setNeedsCompositingGeometryUpdate();
                viewTransitionCapture->setImage(image);
            }
        }
    }

    return { };
}

void ViewTransition::setTypes(Ref<ViewTransitionTypeSet>&& newTypes)
{
    m_types = WTF::move(newTypes);
}

RenderViewTransitionCapture* ViewTransition::viewTransitionNewPseudoForCapturedElement(RenderLayerModelObject& renderer)
{
    auto styleable = Styleable::fromRenderer(renderer);
    if (!styleable)
        return nullptr;
    auto capturedName = protect(styleable->element)->viewTransitionCapturedName(styleable->pseudoElementIdentifier);
    if (capturedName.isNull())
        return nullptr;

    Styleable pseudoStyleable(*renderer.document().documentElement(), Style::PseudoElementIdentifier { PseudoElementType::ViewTransitionNew, capturedName });
    return dynamicDowncast<RenderViewTransitionCapture>(pseudoStyleable.renderer());
}

// https://drafts.csswg.org/css-view-transitions/#page-visibility-change-steps
void ViewTransition::visibilityStateChanged()
{
    if (!document())
        return;

    Ref document = *this->document();
    if (document->hidden()) {
        if (document->activeViewTransition() == this)
            skipViewTransition(Exception { ExceptionCode::InvalidStateError, "Skipping view transition because document visibility state has become hidden."_s });
    } else
        ASSERT(!document->activeViewTransition());
}

void ViewTransition::stop()
{
    if (!document())
        return;

    m_phase = ViewTransitionPhase::Done;

    Ref document = *this->document();
    document->unregisterForVisibilityStateChangedCallbacks(*this);

    if (document->activeViewTransition() == this)
        clearViewTransition();
}

Document* ViewTransition::document() const
{
    return downcast<Document>(scriptExecutionContext());
}

bool ViewTransition::documentElementIsCaptured() const
{
    RefPtr document = this->document();
    if (!document)
        return false;

    RefPtr documentElement = document->documentElement();
    if (!documentElement)
        return false;

    CheckedPtr renderer = documentElement->renderer();
    if (!renderer)
        return false;

    return renderer->capturedInViewTransition();
}

UniqueRef<ViewTransitionParams> ViewTransition::takeViewTransitionParams()
{
    auto params = makeUniqueRef<ViewTransitionParams>();
    params->namedElements.swap(m_namedElements);
    params->initialLargeViewportSize = m_initialLargeViewportSize;
    params->initialPageZoom = m_initialPageZoom;

    return params;
}

TextStream& operator<<(TextStream& ts, ViewTransitionPhase phase)
{
    switch (phase) {
    case ViewTransitionPhase::PendingCapture: ts << "PendingCapture"_s; break;
    case ViewTransitionPhase::CapturingOldState: ts << "CapturingOldState"_s; break;
    case ViewTransitionPhase::UpdateCallbackCalled: ts << "UpdateCallbackCalled"_s; break;
    case ViewTransitionPhase::Animating: ts << "Animating"_s; break;
    case ViewTransitionPhase::Done: ts << "Done"_s; break;
    }
    return ts;
}

}
