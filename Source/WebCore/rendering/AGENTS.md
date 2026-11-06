# WebCore/rendering

Architectural rules that aren't visible from any single file. This file covers how rendering consumes style and how the render tree is built and updated. Read together with `../style/AGENTS.md`.

The bar for changing this file is high. Entries state rules, invariants and direction, not descriptions of current code. If a patch makes an entry inaccurate, fix or delete it in the same patch.

## Consuming style

- Style reaches a renderer only from style resolution, through `RenderTreeUpdater`, at creation or in `RenderElement::setStyle()`. Change handling is driven by diffing old and new style. Layout and painting never write style. They pass used values through layout state. Known exceptions, not to be extended: layout-time sizing of shadow and anonymous children in replaced elements, form controls and list markers, iOS theme border radius adjustment during layout, and text autosizing.
- Used values that depend on the box go through `UsedStyle` (`rendering/style/UsedStyle.h`), so every client sees the same answer. Don't add box-dependent `used*` accessors to `ComputedStyle` and don't construct a `UsedStyle` without a renderer. Code that only has a style uses `Style::effective*()`.
- `UsedStyle` covers used values common to all boxes. Geometry specific to a formatting context, such as collapsed table borders, stays in the renderer and is built on top of `UsedStyle`.
- Renderer state that changes used values without a style change must do its own repaint and layout invalidation. The style diff never sees it.
- State derived from `ComputedStyle` is maintained by its setters, not recomputed by renderers.

## Text

- Text renderers own a full style: inherited properties from the parent, initial values for the rest. Read a parent's non-inherited properties from the parent, never through the text style. A text's first-line style is still shared with its parent, so read only inherited properties from it.
- `display: contents` text is ordinary text whose style inherits from the `display: contents` element. It gets no wrapper boxes and no box geometry of its own. Don't emulate wrappers.
- Direction: text styles are set from `RenderTreeUpdater` like element styles, not propagated from the parent's `setStyle()`.

## Building the render tree

- All render tree mutation goes through `RenderTreeBuilder`.
- When a renderer leaves the tree, everything referring to it or its layout box is purged synchronously. Correctness never depends on delayed destruction.
- Pseudo-element styles that only painting needs are resolved lazily by painting (`lazyPseudoElementStyle()`) and compared by the style diff. Don't teach style resolution which elements need them.
- Optimizations that skip generating boxes fail safe. The default value of any skip bit must mean "may generate a box", so a missed bit costs performance, never a missing box.

## Direction

- `Layout::Box` and `RenderObject` are converging into one object, with layout output moving out of the objects. Put renderer-forwarding accessors on `Layout::Box`, describe its values as the box's own, and prefer removing code from renderers over adding it.
- Layout should not read the DOM. Whatever it needs is copied into the render tree when the tree is updated.
