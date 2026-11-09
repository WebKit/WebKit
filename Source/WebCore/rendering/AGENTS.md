# WebCore/rendering

Architectural rules that aren't visible from any single file. This file covers how rendering consumes style, how the render tree is built and updated, and layout. Read together with `../style/AGENTS.md`.

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

## Terms

- IFC, FFC and GFC: the inline, flex and grid formatting contexts in `layout/formattingContexts/`. Render tree layout: the `layout()` code on renderers.
- The integration (`layout/integration/`) runs a formatting context for a renderer and writes the results into renderers.
- Containing block means `containingBlock()`, which is always a `RenderBlock` and can differ from the CSS containing block.
- Block-start relative: the axes stay physical (x is horizontal in every writing mode), but the block-axis coordinate is measured from the block-start edge, the right or bottom edge when `WritingMode::isBlockFlipped()`.
- Render tree logical left (`logicalLeft()`, `logicalLeftForChild()`) is the left or top side, not the inline start. `direction: rtl` doesn't mirror it.

## Which code lays out a box

- Inline content, SVG text included: IFC (`LayoutIntegration::LineLayout`). There is no other line layout.
- Block-level children of a block container: render tree layout (`RenderBlockFlow::layoutBlockChildren()`). Flex: FFC (`LayoutIntegration::FlexLayout`). `display: -webkit-box` never uses FFC. Tables, MathML and the rest of SVG: their renderers.
- Grid: GFC when `LayoutIntegration::canUseForGridLayout()` accepts the grid, `RenderGrid` otherwise. `gridLayoutAvoidanceReason()` lists why a grid is rejected.
- Out-of-flow boxes: their containing block's renderer, after its in-flow content (`RenderBlock::layoutOutOfFlowBoxes()`).

## Layout

- `Document::updateLayout()` resolves style before layout, except for layout interleaved with style resolution (`../style/AGENTS.md`). Layout and its post-layout tasks run under `ScriptDisallowedScope`: no script, events or DOM mutation. Events layout causes, such as `resize`, are scheduled.
- After layout, no renderer needs layout (`RenderTreeNeedsLayoutChecker` checks in debug builds). Painting requires a clean tree and never lays out. Hit testing updates layout first.
- Intrinsic widths are cached apart from layout and invalidated up the ancestors (`invalidateContentLogicalWidths()`). Computing them doesn't mark the measured box for layout (`SetLayoutNeededForbiddenScope`).
- Shortcuts such as simplified layout or skipping `content-visibility` subtrees fail safe: when unsure, take the full path.
- Style change hooks set what layout has to redo and never clear it. Layout clears it, so a later style change can't cancel an earlier one.
- Don't add renderer APIs for layout queries such as baselines. Answer them in the formatting context or the integration.
- Use css-sizing terms: min-content, max-content, fit-content, and size (the box's own) versus contribution (what it adds to its containing block's intrinsic size). Not "preferred width" or "shrink-to-fit".
- Code implementing a spec algorithm keeps the spec's steps, one function or named lambda each, quoting the step. Comments quote spec text, never test behavior.

## Geometry

- `RenderBox::location()` is relative to the border box of the box that positions it, which is not always the containing block. It is block-start relative. Painting, hit testing and layers are physical, and a `RenderBox` parent converts a child's location with `blockStartRelativeToPhysicalForChild()`.
- A containing block reads child geometry with its `logical*ForChild()` and `margin*ForChild()` accessors, in its own writing mode. The child's `logicalTop()`, `logicalWidth()` and friends use the child's, so they swap axes in an orthogonal flow.
- Of its box geometry, a `RenderBox` stores the border box rect and the margins (by physical side). Border and padding are computed when read. `borderBoxRect()`, `paddingBoxRect()` and `contentBoxRect()` are physical and local to the border box. `paddingBoxRect()` excludes scrollbars and `paddingBoxRectIncludingScrollbar()` doesn't.
- Overflow rects are block-start relative in the box's own writing mode. Ink overflow (`visualOverflowRect()`) is for painting, hit-test culling and invalidation and never affects sizes or positions. Scrollable overflow (`layoutOverflowRect()`) sets the scroll extent.
- Inline boxes (`isInlineBox()`) and text have no box geometry, only display boxes, one or more per line. Read them through `InlineIterator`, whose `visualRect()` is physical in the IFC root's border box coordinates. Only IFC and the inline painters touch display boxes directly.
- Convert coordinates with `localToAbsolute()`, `absoluteToLocal()` and friends, never by adding up `location()`s: relative and sticky offsets, scroll offsets, transforms and fragmentation are not in them. Inside a fragmented flow, render tree geometry is the position in the `RenderFragmentedFlow`'s single strip, not where the box is displayed.

## Direction

- `Layout::Box` and `RenderObject` are converging into one object, with layout output moving out of the objects. Put renderer-forwarding accessors on `Layout::Box`, describe its values as the box's own, and prefer removing code from renderers over adding it.
- Layout should not read the DOM. Whatever it needs is copied into the render tree when the tree is updated.

## Testing

- New layout tests are WPT reftests in `LayoutTests/imported/w3c/web-platform-tests/`, under the spec they test (`css/<module>/`, `mathml/`, `svg/`). Name the reference `<test>-ref.html` so the harness doesn't run it as a test, and add a `-expected` copy of it, which is what WebKit compares against. The reference renders the expected result without the feature under test.
- Tests using `internals` go in WebKit test directories. Tests for stale layout or stale intrinsic sizes don't load fonts from a URL: the load forces a full style rebuild (`Document::fontsNeedUpdate()`), which hides the bug.

## Debugging

- In debug builds, `showRenderTree(&renderer)` prints the whole tree with a legend, marks the renderer with `*`, and shows why each renderer needs layout.
- In debug builds, `notifyutil -p com.apple.WebKit.showLegacyGridReasons` prints, with `WTFLogAlways`, why the document's grids are not laid out by GFC. It works once a grid has been checked.
- To compare GFC with `RenderGrid`, run the test with GFC off: `run-webkit-tests --internal-feature GridFormattingContextIntegrationEnabled=false <test>`. IFC and FFC have no other path to compare with.
