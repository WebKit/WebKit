# LFC Display tree

This document describes the Layout Formatting Context display tree and related functionality (aka "LFC Display").


## Basic concepts

The **display tree** is intended to be a fully-resolved, isolated tree that is suitable for painting and hit-testing.

*Fully-resolved* means that all style values have been resolved to the values use for painting. For example, colors in the display tree are those which result from the application of visited link rules, and from the application of `-apple-color-filter`.

*Isolated* means that a display tree is a stand-alone data structure that can be painted without reference to data structures share with layout. For example, the display tree does not use `RenderStyle` once built, but constructs its own display styles at tree-building time.

All the geometry in the display tree is already in painting coordinates: pixel snapping happens at tree building time.

In general the tree has been designed to push as much of the complexity to tree-building time as possible, so that painting is fast.

The tree objects themselves are intentionally lightweight, mostly **data-only** objects. Complex tree-walking code lives in external objects that know how to do certain things, like paint the box tree.

Boxes in the display tree are intended to be fairly generic, and not have properties which are very CSS- or SVG-specific. It's hoped that SVG and CSS rendering share many of the same box classes.

There isn't necessarily a 1:1 relationship between `Layout::Box` and `Display::Box`. The display tree may elide boxes that paint nothing, or may create extra boxes for functionality like scrolling.

## Relevant classes

### Display::View

Represents a presentation of a display tree. Is the entry point for hit-testing.

Currently `FrameView` owns a `Display::View`.

### Display::Tree

Owns the hierarchy of `Display::Box` objects via `Display::StackingItem` objects, and associated tree-related data structures. It should be possible for a `Display::Tree` to exist independently of a `Display::View`.

### Display::TreeBuilder

`Display::TreeBuilder` is a short-lived class that knows how to build a `Display::StackingItem`/`Display::Box` tree from a `Layout::Box` tree, `LayoutState` and style. It maintains various bits of state in a stack as it's building the display tree in order to track containing blocks etc. `Display::TreeBuilder` knows about hierarchy, not geometry.

### Display::BoxFactory

`Display::BoxFactory` actually creates the `Display::Box` objects and their related objects like `Display::BoxGeometry`. It exists to keep the style and geometry code out of `Display::TreeBuilder`. It only ever deals with a single box at a time.

### Display::Box

`Display::Box` is the basic building block for the display tree; it represents some rectangular region, often with painted content. There are subclasses for text, images etc. `Display::Box` has sibling pointers and a parent pointer. `Display::ContainerBox` is used for boxes with children, so has a pointer to its first child.

The hierarchy of `Display::Box` objects forms a tree, but it's not a complete tree, because each `Display::StackingItem` owns a part of the tree, and the box objects in those subtrees are not connected.

### Display::StackingItem

This class exists to represent parts of the box tree that are painted atomically, as specified in [CSS 2.2 Appendix E](https://www.w3.org/TR/CSS22/zindex.html). There is always a `Display::StackingItem` at the root (via which the `Display::Tree` owns the actual box tree), and CSS positioning and other styles which cause CSS stacking context trigger the creation of a `Display::StackingItem`. `Display::StackingItem` objects that represent CSS stacking contexts have *positive* and *negative z-order lists* which contain owning references to the descendant `Display::StackingItem` objects. These lists are built and sorted at tree-building time.

### Display::Style

`Display::Style` is the display tree's equivalent of `RenderStyle` but with some important differences. Every `Display::Box` has a `Display::Style`, so it should be relatively small. It must only contain data that is independent of box geometry; i.e. it must not contain any `Length` values (which can hide things like `calc()` values that must have been already resolved). It should be *shareable* between boxes with the same appearance, but possibly different sizes. All colors should be their final used values, i.e. after resolving `visitedDependentColor()` and applying `-apple-color-filter`.

### Display::BoxRareGeometry / Display::BoxDecorationData

These classes store geometry-dependent data specific to a single `Display::Box`, and are only allocated for boxes that have the relevant styles (like visible borders, or border-radius).

`Display::BoxDecorationData` stores information about backgrounds and borders.

`Display::BoxRareGeometry` stores border-radius (here because a box with overflow:hidden clips to its border-radius even without any border), and transforms.

### Display::CSSPainter

`Display::CSSPainter` knows how to walk the trees of `Display::StackingItem` and their `Display::Box` objects in order to implement CSS painting. It knows how to apply clipping, opacity and filters.

For painting individual boxes, `Display::CSSPainter` makes use of `Display::BoxDecorationPainter` which knows how to paint CSS borders and backgrounds.

`Display::CSSPainter` objects exist on the stack only during painting; they are not long-lived.

It's intended that SVG would be painted by a `Display::SVGPainter` class at some point.

Hit-testing is very similar to painting, so that code lives in `Display::CSSPainter` too. Maybe it needs a different name.

## Painting

The entry point to painting some subset of the display tree is a `GraphicsLayerClient`'s `paintContents()` paint callback. Eventually the display tree will be presented via a hierarchy of GraphicsLayers; for now, there is a single GraphicsLayer at the root.

To paint the tree, the static `CSSPainter::paintTree()` function is called, and `Display::CSSPainter` traverses the `Display::StackingItem` hierarchy and paints their `Display::Box` objects, making use of `Display::BoxDecorationPainter` on boxes with box decorations.

At CSS/SVG boundaries a new painter object will be created on the stack to paint the inner content.

The destination `GraphicsContext` is passed around as an argument to painting functions, wrapped up in a `PaintingContext` struct that also has the `deviceScaleFactor` and will probably include a dirty rect. 

## Geometry

	Note: this is subject to change. The code needs to make it hard to use the absolute rects in the wrong way.

Display boxes store their position and size as an absolute (i.e. document-relative) `FloatRect` (already pixel-snapped). CSS box-model boxes, which have things like borders and border-radius, also store rects for their padding and content boxes. Thus no paint offsets have to be tracked during painting.

Some boxes act as *coordinate boundaries*; namely those with transforms, and those which are affected by scrolling. The "absolute" rects are computed before accounting for scrolling and transforms, so code that needs to map a box's rect into view coordinate will need to take those into account.

## Clipping

Clipping is a source of complexity in the display tree because CSS clipping does not trigger CSS stacking context; the clipping tree follows the containing block tree, which is different from paint order. For painting normal in-flow content we can just apply clipping at paint time, but when painting out-of-flow content, we have to be able to compute the clip from ancestors. Such out-of-flow content is always represented by the root box of a `Display::StackingItem`.

At tree-building time `Display::TreeBuilder` keeps track of containing blocks, so for boxes which initiate out-of-order painting (i.e. those which are the root box of a `Display::StackingItem`) `Display::BoxFactory` can ask the containing block for the clip that should be applied.

Clips are store in `Display::BoxClip` which tracks an intersection rect, and, if necessary, a set of rounded rects needed to apply border-radius clips from ancestors. `Display::BoxClip` objects are shared between parent/child `Display::BoxModelObject` objects that share the same clip (i.e. have no overflow of their own).


## Scrolling

	Note: this is preliminary

A `Layout::Box` with `overflow:auto` or `overflow:scroll` is represented with a hierarchy of three display boxes:

```
container box
	scrolling container box
		scrolled contents box

```
(and possibly additional boxes for scrollbars and the scroll corner).

The container box exists to paint box decorations on the scrolling element. The "scrolling container" box has geometry of the container's *content box*, and exists to represent the scroll offset. The "scrolled contents" box paints the scrolled content, and has the size of the layout overflow.

It is a goal of the display tree that scrolling should not invalidate any geometry on the subtree (thus avoiding post-scroll tree walks to fix up geometry, which are known to be a source of performance issues in rendering code).

	Note: explain how scrolling interacts with positioned elements.

## Compositing

	Note: write me

