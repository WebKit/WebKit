Visual formatting model implementation.
See https://www.w3.org/TR/CSS22/visuren.html for more information.

WebCore
1. recursive layout
2. layout logic lives in the renderers mixing block with inline etc.
3. No clear separation of logic and state.

LayoutReloaded
1. Iterative layout within a formatting context, recursive across nested formatting contexts 
2. Formatting context is responsible for computing size/positions for all the boxes that live in the
context including in/out-of-flow and floating boxes
3. Layout state is stored in dedicated FormattingState objects.
3. Initial containing block creates the top level (initial) block formatting context
4. Floats are resitriced to their formatting contexts.
5. Boxes, including inline tree are generated while laying out the content. LayoutTree -> BoxTree.

Instructions:
1. apply ./misc/LayoutReloadedWebKit.patch
2. compile WebKit
3. load ./test/index.html

Partially done:
Block formatting context:
- static, relative and out of flow positioning
- margin collapsing
- floats

Missing:
- Inline formatting context
- Everything else
