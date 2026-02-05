/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
#include "InteractionRegion.h"

#include "AXObjectCache.h"
#include "AccessibilityObject.h"
#include "BorderShape.h"
#include "ContainerNodeInlines.h"
#include "Document.h"
#include "ElementAncestorIteratorInlines.h"
#include "ElementInlines.h"
#include "ElementRuleCollector.h"
#include "FloatSizeHash.h"
#include "FrameSnapshotting.h"
#include "GeometryUtilities.h"
#include "HTMLAnchorElement.h"
#include "HTMLAttachmentElement.h"
#include "HTMLButtonElement.h"
#include "HTMLFieldSetElement.h"
#include "HTMLFormControlElement.h"
#include "HTMLInputElement.h"
#include "HTMLLabelElement.h"
#include "HitTestResult.h"
#include "LayoutShape.h"
#include "LegacyRenderSVGShape.h"
#include "LegacyRenderSVGShapeInlines.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "NativeImage.h"
#include "NodeList.h"
#include "Page.h"
#include "PathOperation.h"
#include "PlatformMouseEvent.h"
#include "PseudoClassChangeInvalidation.h"
#include "RenderAncestorIterator.h"
#include "RenderBoxInlines.h"
#include "RenderImage.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderObjectStyle.h"
#include "RenderVideo.h"
#include "SVGSVGElement.h"
#include "Settings.h"
#include "SimpleRange.h"
#include "SliderThumbElement.h"
#include "StyleResolver.h"
#include "TextIterator.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/MakeString.h>

#if ENABLE(FORM_CONTROL_REFRESH)
#include "PathCG.h"
#include "RenderTheme.h"
#endif

namespace WebCore {

InteractionRegion::~InteractionRegion() = default;

class InteractionRegionPathCache {
public:
    static InteractionRegionPathCache& singleton();

    std::optional<Path> get(const Image&, const FloatSize&);
    void add(const Image&, const FloatSize&, Path);

    void clear();

private:
    friend class NeverDestroyed<InteractionRegionPathCache>;

    InteractionRegionPathCache() = default;

    WeakHashMap<const Image, HashMap<FloatSize, Path>> m_imageCache;
};

InteractionRegionPathCache& InteractionRegionPathCache::singleton()
{
    static NeverDestroyed<InteractionRegionPathCache> cache;
    return cache;
}

std::optional<Path> InteractionRegionPathCache::get(const Image& image, const FloatSize& size)
{
    if (auto cacheBySize = m_imageCache.getOptional(image))
        return cacheBySize->getOptional(size);
    return std::nullopt;
}

void InteractionRegionPathCache::add(const Image& image, const FloatSize& size, Path path)
{
    m_imageCache.ensure(image, [] {
        return HashMap<FloatSize, Path>();
    }).iterator->value.add(size, path);
}

void InteractionRegionPathCache::clear()
{
    m_imageCache.clear();
}

void InteractionRegion::clearCache()
{
    InteractionRegionPathCache::singleton().clear();
}

static bool hasInteractiveCursorType(Element& element)
{
    auto* renderer = element.renderer();
    auto* style = renderer ? &renderer->style() : nullptr;
    auto cursorType = style ? style->cursorType() : CursorType::Auto;

    if (cursorType == CursorType::Auto && element.enclosingLinkEventParentOrSelf())
        cursorType = CursorType::Pointer;

    return cursorType == CursorType::Move
        || cursorType == CursorType::Pointer
        || cursorType == CursorType::Text
        || cursorType == CursorType::VerticalText;
}

static bool shouldAllowElement(const Element& element)
{
    if (is<HTMLFieldSetElement>(element))
        return false;

    if (auto* input = dynamicDowncast<HTMLInputElement>(element)) {
        if (input->isDisabledFormControl())
            return false;

        // Do not allow regions for the <input type='range'>, because we make one for the thumb.
        if (input->isRangeControl())
            return false;

        // Do not allow regions for the <input type='file'>, because we make one for the button.
        if (input->isFileUpload())
            return false;
    }

    return true;
}

static bool shouldAllowAccessibilityRoleAsPointerCursorReplacement(const Element& element)
{
    switch (AccessibilityObject::ariaRoleToWebCoreRole(element.attributeWithoutSynchronization(HTMLNames::roleAttr))) {
    case AccessibilityRole::Button:
    case AccessibilityRole::Checkbox:
    case AccessibilityRole::Link:
    case AccessibilityRole::ListBoxOption:
    case AccessibilityRole::MenuItem:
    case AccessibilityRole::MenuItemCheckbox:
    case AccessibilityRole::MenuItemRadio:
    case AccessibilityRole::PopUpButton:
    case AccessibilityRole::RadioButton:
    case AccessibilityRole::Slider:
    case AccessibilityRole::Switch:
    case AccessibilityRole::TextField:
    case AccessibilityRole::ToggleButton:
        return true;
    default:
        return false;
    }
}

static bool elementMatchesHoverRules(Element& element)
{
    bool foundHoverRules = false;
    bool initialValue = element.isUserActionElement() && element.document().userActionElements().isHovered(element);

    for (auto key : Style::makePseudoClassInvalidationKeys(CSSSelector::PseudoClass::Hover, element)) {
        auto& ruleSets = element.styleResolver().ruleSets();
        auto* invalidationRuleSets = ruleSets.pseudoClassInvalidationRuleSets(key);
        if (!invalidationRuleSets)
            continue;

        for (auto& invalidationRuleSet : *invalidationRuleSets) {
            element.document().userActionElements().setHovered(element, invalidationRuleSet.isNegation == Style::IsNegation::No);
            Style::ElementRuleCollector ruleCollector(element, *invalidationRuleSet.ruleSet, nullptr, SelectorChecker::Mode::StyleInvalidation);
            if (ruleCollector.matchesAnyAuthorRules()) {
                foundHoverRules = true;
                break;
            }
        }

        if (foundHoverRules)
            break;
    }

    element.document().userActionElements().setHovered(element, initialValue);
    return foundHoverRules;
}

static bool shouldAllowNonInteractiveCursorForElement(const Element& element)
{
#if ENABLE(ATTACHMENT_ELEMENT)
    if (is<HTMLAttachmentElement>(element))
        return true;
#endif

    if (RefPtr textElement = dynamicDowncast<HTMLTextFormControlElement>(element))
        return !textElement->focused() || !textElement->lastChangeWasUserEdit() || textElement->value()->isEmpty();

    if (is<HTMLFormControlElement>(element))
        return true;

    if (is<SliderThumbElement>(element))
        return true;

    if (is<HTMLAnchorElement>(element))
        return true;

    if (shouldAllowAccessibilityRoleAsPointerCursorReplacement(element))
        return true;

    return false;
}

static bool shouldGetOcclusion(const RenderElement& renderer)
{
    if (auto* renderLayerModelObject = dynamicDowncast<RenderBox>(renderer)) {
        if (renderLayerModelObject->hasLayer() && renderLayerModelObject->layer()->isComposited())
            return false;
    }

    if (auto specifiedZIndexValue = renderer.style().specifiedZIndex().tryValue(); specifiedZIndexValue && *specifiedZIndexValue > 0)
        return true;

    if (renderer.isFixedPositioned())
        return true;

    return false;
}

static bool hasTransparentContainerStyle(const RenderStyle& style)
{
    return !style.hasBackground()
        && !style.hasOutline()
        && !style.hasBoxShadow()
        && !style.hasClipPath()
        && !style.hasExplicitlySetBorderRadius()
        // No visible borders or borders that do not create a complete box.
        && (!style.hasVisibleBorder()
            || !(style.usedBorderTopWidth() && style.usedBorderRightWidth() && style.usedBorderBottomWidth() && style.usedBorderLeftWidth()));
}

static bool canTweakShapeForStyle(const RenderStyle& style)
{
    if (!hasTransparentContainerStyle(style))
        return false;

    switch (style.usedAppearance()) {
    case StyleAppearance::TextField:
    case StyleAppearance::TextArea:
        return false;
    default:
        return true;
    }
}

static bool colorIsChallengingToHighlight(const Color& color)
{
    constexpr double luminanceThreshold = 0.01;

    return color.isValid()
        && ((color.luminance() < luminanceThreshold || std::abs(color.luminance() - 1) < luminanceThreshold));
}

static bool styleIsChallengingToHighlight(const RenderStyle& style)
{
    auto color = (style.fill().isNone() ? style.stroke() : style.fill()).tryColor();
    if (!color)
        return false;

    Style::ColorResolver colorResolver { style };
    return colorIsChallengingToHighlight(colorResolver.colorResolvingCurrentColor(*color));
}

static bool isGuardContainer(const Element& element)
{
    bool isButton = is<HTMLButtonElement>(element);
    bool isLink = element.isLink();
    if (!isButton && !isLink)
        return false;

    if (!element.firstElementChild()
        || element.firstElementChild() != element.lastElementChild())
        return false;

    if (!element.renderer())
        return false;

    auto& renderer = *element.renderer();
    return hasTransparentContainerStyle(renderer.style());
}

static FloatSize boundingSize(const RenderObject& renderer, const std::optional<AffineTransform>& transform)
{
    Vector<LayoutRect> rects;
    renderer.boundingRects(rects, LayoutPoint());

    if (!rects.size())
        return FloatSize();

    FloatSize size = unionRect(rects).size();
    if (transform)
        size.scale(transform->xScale(), transform->yScale());

    return size;
}

static bool cachedImageIsPhoto(const CachedImage& cachedImage)
{
    if (cachedImage.errorOccurred())
        return false;

    RefPtr image = cachedImage.image();
    if (!image || !image->isBitmapImage())
        return false;

    if (image->nativeImage() && image->nativeImage()->hasAlpha())
        return false;

    return true;
}

static RefPtr<Image> findIconImage(const RenderObject& renderer)
{
    if (const auto& renderImage = dynamicDowncast<RenderImage>(renderer)) {
        if (!renderImage->cachedImage() || renderImage->cachedImage()->errorOccurred())
            return nullptr;

        RefPtr image = renderImage->cachedImage()->imageForRenderer(renderImage);
        if (!image)
            return nullptr;

        if (image->isSVGImageForContainer()
            || (image->isBitmapImage() && image->nativeImage() && image->nativeImage()->hasAlpha()))
            return image;
    }

    return nullptr;
}

static std::optional<std::pair<Ref<SVGSVGElement>, Ref<SVGGraphicsElement>>> findSVGClipElements(const RenderObject& renderer)
{
    if (const auto& renderShape = dynamicDowncast<LegacyRenderSVGShape>(renderer)) {
        Ref shapeElement = renderShape->graphicsElement();
        if (auto* owner = shapeElement->ownerSVGElement()) {
            Ref svgSVGElement = *owner;
            return std::make_pair(svgSVGElement, shapeElement);
        }
    }

    return std::nullopt;
}

#if ENABLE(INTERACTION_REGION_TEXT_CONTENT)
static String interactionRegionTextContentForNode(Node& node)
{
    if (auto nodeRange = makeRangeSelectingNode(node))
        return plainText(*nodeRange);
    return { };
}
#endif

std::optional<InteractionRegion> interactionRegionForRenderedRegion(const RenderObject& regionRenderer, const FloatRect& bounds, const FloatSize& clipOffset, const std::optional<AffineTransform>& transform)
{
    if (bounds.isEmpty())
        return std::nullopt;

    if (!regionRenderer.node())
        return std::nullopt;

    RefPtr originalElement = dynamicDowncast<Element>(regionRenderer.node());
    if (originalElement && originalElement->isPseudoElement())
        return std::nullopt;

    RefPtr matchedElement = originalElement;
    if (!matchedElement)
        matchedElement = regionRenderer.node()->parentElement();
    if (!matchedElement)
        return std::nullopt;

    bool isLabelable = [&] {
        RefPtr htmlElement = dynamicDowncast<HTMLElement>(matchedElement);
        return htmlElement && htmlElement->isLabelable();
    }();
    for (RefPtr<ContainerNode> node = matchedElement; node; node = node->parentInComposedTree()) {
        RefPtr element = dynamicDowncast<Element>(node);
        if (!element)
            continue;
        bool matchedButton = is<HTMLButtonElement>(*element);
        bool matchedLabel = isLabelable && is<HTMLLabelElement>(*element);
        bool matchedLink = element->isLink();
        if (matchedButton || matchedLabel || matchedLink) {
            matchedElement = element;
            break;
        }
    }

    if (!shouldAllowElement(*matchedElement))
        return std::nullopt;

    if (!matchedElement->renderer())
        return std::nullopt;
    auto& renderer = *matchedElement->renderer();

    if (renderer.usedPointerEvents() == PointerEvents::None)
        return std::nullopt;

    bool isOriginalMatch = matchedElement == originalElement;

    // FIXME: Consider also allowing elements that only receive touch events.
    bool hasListener = renderer.style().eventListenerRegionTypes().contains(EventListenerRegionType::MouseClick);
    bool hasPointer = hasInteractiveCursorType(*matchedElement) || shouldAllowNonInteractiveCursorForElement(*matchedElement);

    RefPtr localMainFrame = dynamicDowncast<LocalFrame>(regionRenderer.document().frame()->mainFrame());
    if (!localMainFrame) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }
    RefPtr pageView = localMainFrame->view();
    if (!pageView) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    auto viewportSize = FloatSize(pageView->baseLayoutViewportSize());
    auto viewportArea = viewportSize.area();
    bool isTooBigForInteraction = bounds.area() > viewportArea / 3;
    bool isTooBigForOcclusion = bounds.area() > viewportArea * 3;

    auto nodeIdentifier = matchedElement->nodeIdentifier();

    if (!hasPointer) {
        if (RefPtr labelElement = dynamicDowncast<HTMLLabelElement>(matchedElement)) {
            // Could be a `<label for="...">` or a label with a descendant.
            // In cases where both elements get a region we want to group them by the same `nodeIdentifier`.
            auto associatedElement = labelElement->control();
            if (associatedElement && !associatedElement->isDisabledFormControl()) {
                hasPointer = true;
                nodeIdentifier = associatedElement->nodeIdentifier();
            }
        }
    }

    bool detectedHoverRules = false;
    if (!hasPointer) {
        // The hover check can be expensive (it may end up doing selector matching), so we only run it on some elements.
        bool hasVisibleBoxDecorations = renderer.hasVisibleBoxDecorations();
        bool nonScrollable = [&] {
            auto* box = dynamicDowncast<RenderBox>(renderer);
            return !box || (!box->hasScrollableOverflowX() && !box->hasScrollableOverflowY());
        }();
        if (hasVisibleBoxDecorations && nonScrollable)
            detectedHoverRules = elementMatchesHoverRules(*matchedElement);
    }

    if (!hasListener || !(hasPointer || detectedHoverRules) || isTooBigForInteraction) {
        if (isOriginalMatch && shouldGetOcclusion(renderer) && !isTooBigForOcclusion) {
            return { {
                InteractionRegion::Type::Occlusion,
                nodeIdentifier,
                bounds
            } };
        }

        return std::nullopt;
    }

    bool isInlineNonBlock = renderer.isInline() && !renderer.isBlockLevelReplacedOrAtomicInline();
    bool isPhoto = false;

    float minimumContentHintArea = 200 * 200;
    bool needsContentHint = bounds.area() > minimumContentHintArea;
    if (needsContentHint) {
        if (auto* renderImage = dynamicDowncast<RenderImage>(regionRenderer)) {
            isPhoto = [&]() -> bool {
#if ENABLE(VIDEO)
                if (is<RenderVideo>(renderImage))
                    return true;
#endif
                if (!renderImage->cachedImage())
                    return false;

                return cachedImageIsPhoto(*renderImage->cachedImage());
            }();
        } else if (regionRenderer.style().hasBackgroundImage()) {
            isPhoto = [&]() -> bool {
                RefPtr backgroundImage = regionRenderer.style().backgroundLayers().usedFirst().image().tryStyleImage();
                if (!backgroundImage || !backgroundImage->cachedImage())
                    return false;

                return cachedImageIsPhoto(*backgroundImage->cachedImage());
            }();
        }
    }

    bool matchedElementIsGuardContainer = isGuardContainer(*matchedElement);

    if (isOriginalMatch && matchedElementIsGuardContainer) {
        return { {
            InteractionRegion::Type::Guard,
            nodeIdentifier,
            bounds
        } };
    }

    // The parent will get its own InteractionRegion.
    if (!isOriginalMatch && !matchedElementIsGuardContainer && !isPhoto && !isInlineNonBlock && !renderer.style().isDisplayTableOrTablePart())
        return std::nullopt;

    // FIXME: Consider allowing rotation / skew - rdar://127499446.
    bool hasRotationOrShear = false;
    if (transform)
        hasRotationOrShear = transform->isRotateOrShear();

    RefPtr<Image> iconImage;
    std::optional<std::pair<Ref<SVGSVGElement>, Ref<SVGGraphicsElement>>> svgClipElements;
    if (!hasRotationOrShear && !needsContentHint)
        iconImage = findIconImage(regionRenderer);
    if (!hasRotationOrShear && !iconImage)
        svgClipElements = findSVGClipElements(regionRenderer);

    auto rect = bounds;
    float cornerRadius = 0;
    OptionSet<InteractionRegion::CornerMask> maskedCorners { };
    std::optional<Path> clipPath = std::nullopt;

    auto& style = regionRenderer.style();
    CheckedPtr<const RenderBox> regionRendererBox;

    if (auto basicShapePath = style.clipPath().tryBasicShape(); !hasRotationOrShear && originalElement && basicShapePath) {
        auto size = boundingSize(regionRenderer, transform);
        auto path = Style::tryPath(*basicShapePath, TransformOperationData(FloatRect(FloatPoint(), size)));

        if (path && !clipOffset.isZero())
            path->translate(clipOffset);

        clipPath = path;
    } else if (iconImage && originalElement) {
        auto size = boundingSize(regionRenderer, transform);
        auto generateAndCachePath = [&] {
            LayoutRect imageRect(FloatPoint(), size);
            Ref shape = LayoutShape::createRasterShape(iconImage.get(), 0, imageRect, imageRect, WritingMode(), 0);
            LayoutShape::DisplayPaths paths;
            shape->buildDisplayPaths(paths);
            auto path = paths.shape;
            InteractionRegionPathCache::singleton().add(*iconImage.get(), size, path);
            return path;
        };
        auto cachedPath = InteractionRegionPathCache::singleton().get(*iconImage.get(), size);
        auto path = cachedPath ? *cachedPath : generateAndCachePath();

        if (!clipOffset.isZero())
            path.translate(clipOffset);

        clipPath = path;
    } else if (svgClipElements) {
        auto& [svgSVGElement, shapeElement] = *svgClipElements;
        auto path = shapeElement->toClipPath();

        FloatSize size = svgSVGElement->currentViewportSizeExcludingZoom();
        auto viewBoxTransform = svgSVGElement->viewBoxToViewTransform(size.width(), size.height());

        auto shapeBoundingBox = shapeElement->getBBox(SVGLocatable::DisallowStyleUpdate);
        path.transform(viewBoxTransform);
        shapeBoundingBox = viewBoxTransform.mapRect(shapeBoundingBox);

        constexpr float smallShapeDimension = 30;
        bool shouldFallbackToContainerRegion = shapeBoundingBox.size().minDimension() < smallShapeDimension
            && styleIsChallengingToHighlight(style)
            && matchedElementIsGuardContainer;

        // Bail out, we'll convert the guard container to Interaction.
        if (shouldFallbackToContainerRegion)
            return std::nullopt;

        path.translate(FloatSize(-shapeBoundingBox.x(), -shapeBoundingBox.y()));

        if (!clipOffset.isZero())
            path.translate(clipOffset);

        clipPath = path;
    } else if ((regionRendererBox = dynamicDowncast<RenderBox>(regionRenderer))) {
        auto borderShape = BorderShape::shapeForBorderRect(regionRendererBox->style(), regionRendererBox->borderBoxRect());
        auto borderRadii = borderShape.radii();
        auto minRadius = borderRadii.minimumRadius();
        auto maxRadius = borderRadii.maximumRadius();

        bool needsClipPath = false;
        auto checkIfClipPathNeeded = [&](auto radius) {
            if (radius != minRadius && radius != maxRadius)
                needsClipPath = true;
        };
        checkIfClipPathNeeded(borderRadii.topLeft().minDimension());
        checkIfClipPathNeeded(borderRadii.topRight().minDimension());
        checkIfClipPathNeeded(borderRadii.bottomLeft().minDimension());
        checkIfClipPathNeeded(borderRadii.bottomRight().minDimension());

        if (minRadius == maxRadius)
            cornerRadius = minRadius;
        else if (!minRadius && !needsClipPath) {
            cornerRadius = maxRadius;
            if (borderRadii.topLeft().minDimension() == maxRadius)
                maskedCorners.add(InteractionRegion::CornerMask::MinXMinYCorner);
            if (borderRadii.topRight().minDimension() == maxRadius)
                maskedCorners.add(InteractionRegion::CornerMask::MaxXMinYCorner);
            if (borderRadii.bottomLeft().minDimension() == maxRadius)
                maskedCorners.add(InteractionRegion::CornerMask::MinXMaxYCorner);
            if (borderRadii.bottomRight().minDimension() == maxRadius)
                maskedCorners.add(InteractionRegion::CornerMask::MaxXMaxYCorner);
        } else
            clipPath = borderShape.pathForOuterShape(regionRendererBox->document().deviceScaleFactor());
    }

    bool canTweakShape = !isPhoto
        && !clipPath
        && canTweakShapeForStyle(style);

    auto adjustForTheme = false;
    auto useContinuousCorners = false;

#if ENABLE(FORM_CONTROL_REFRESH)
    // Certain form controls with native appearance need to use a modified interaction region in order to
    // be shaped correctly. Check for them and use the correpsonding adjusted corner style, size, and radius,
    // if available.
    // FIXME: <rdar://154930959> The region for native textareas still needs to be adjusted.

    if (!regionRendererBox)
        regionRendererBox = dynamicDowncast<RenderBox>(regionRenderer);

    adjustForTheme = regionRendererBox
        && regionRendererBox->settings().formControlRefreshEnabled()
        && !style.hasTransformRelatedProperty();

    if (adjustForTheme) {
        // We only need the bounding path for the region if a clip path exists so that we can compute
        // the intersecting path and use it instead.
        const auto needsPath = clipPath ? ShouldComputePath::Yes : ShouldComputePath::No;
        if (auto themeShape = RenderThemeCocoa::shapeForInteractionRegion(*regionRendererBox, regionRendererBox->borderBoxRect(), needsPath)) {
            if (themeShape->cornerType == CornerType::Continuous)
                useContinuousCorners = true;

            cornerRadius = themeShape->cornerRadius;
            maskedCorners = { };

            if (auto themePath = themeShape->path) {
                // A clip path has already been set. Find the intersecting path between the existing
                // clip path and the bounding path for the adjusted interaction region for the control.
                Path adjustedPath = *themePath;
                if (!clipOffset.isZero())
                    adjustedPath.translate(clipOffset);

                RetainPtr intersectingPath = adoptCF(CGPathCreateCopyByIntersectingPath(adjustedPath.platformPath(), clipPath->platformPath(), false));
                clipPath = { PathCG::create(adoptCF(CGPathCreateMutableCopy(intersectingPath.get()))) };

                // No need for continuous corners if we're already going to clip.
                useContinuousCorners = false;
            }

            // Expand the interaction region by the width of the CSS border, if necessary.
            const auto rectOffset = RenderThemeCocoa::inflateRectForInteractionRegion(*regionRendererBox, rect);
            if (clipPath && !rectOffset.isZero())
                clipPath->translate(rectOffset);
        } else
            adjustForTheme = false;
    }
#endif

    if (canTweakShape && !adjustForTheme) {
        // We can safely tweak the bounds and radius without causing visual mismatch.
        cornerRadius = std::max<float>(cornerRadius, regionRenderer.document().settings().interactionRegionMinimumCornerRadius());
        if (isInlineNonBlock)
            rect.inflate(regionRenderer.document().settings().interactionRegionInlinePadding());
    }

    return { {
        InteractionRegion::Type::Interaction,
        nodeIdentifier,
        rect,
        cornerRadius,
        maskedCorners,
        isPhoto ? InteractionRegion::ContentHint::Photo : InteractionRegion::ContentHint::Default,
        clipPath,
        useContinuousCorners,
#if ENABLE(INTERACTION_REGION_TEXT_CONTENT)
        interactionRegionTextContentForNode(*regionRenderer.node())
#endif
    } };
}

TextStream& operator<<(TextStream& ts, const InteractionRegion& interactionRegion)
{
    auto regionName = interactionRegion.type == InteractionRegion::Type::Interaction
        ? "interaction"_s
        : (interactionRegion.type == InteractionRegion::Type::Occlusion ? "occlusion"_s : "guard"_s);
    ts.dumpProperty(regionName, interactionRegion.rectInLayerCoordinates);
    if (interactionRegion.contentHint != InteractionRegion::ContentHint::Default)
        ts.dumpProperty("content hint"_s, "photo"_s);
    auto radius = interactionRegion.cornerRadius;
    if (radius > 0) {
        if (interactionRegion.maskedCorners.isEmpty()) {
            ts.dumpProperty("cornerRadius"_s, radius);
            auto useContinuousCorners = interactionRegion.useContinuousCorners;
            if (useContinuousCorners)
                ts.dumpProperty("useContinuousCorners"_s, true);
        } else  {
            auto mask = interactionRegion.maskedCorners;
            ts.dumpProperty("cornerRadius"_s, makeString(
                mask.contains(InteractionRegion::CornerMask::MinXMinYCorner) ? radius : 0, ' ',
                mask.contains(InteractionRegion::CornerMask::MaxXMinYCorner) ? radius : 0, ' ',
                mask.contains(InteractionRegion::CornerMask::MaxXMaxYCorner) ? radius : 0, ' ',
                mask.contains(InteractionRegion::CornerMask::MinXMaxYCorner) ? radius : 0
            ));
        }
    }
    if (interactionRegion.clipPath)
        ts.dumpProperty("clipPath"_s, interactionRegion.clipPath.value());
#if ENABLE(INTERACTION_REGION_TEXT_CONTENT)
    if (!interactionRegion.text.isEmpty())
        ts.dumpProperty("text"_s, interactionRegion.text);
#endif
    return ts;
}

}
