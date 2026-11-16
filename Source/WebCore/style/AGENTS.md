# WebCore/style

Architectural rules that aren't visible from any single file. Read together with `../css/AGENTS.md`.

The bar for changing this file is high. Entries state rules, invariants and direction, not descriptions of current code. If a patch makes an entry inaccurate, fix or delete it in the same patch.

## Invariants

- Resolved style is immutable. Render tree updates, layout and repaint are driven by diffing the new style against the renderer's current one (`StyleDifference`). Writing through `mutableStyle()` from layout or painting corrupts that diff. Pass used values through layout state and fix the writer instead of compensating in `RenderTreeUpdater` or the diff.
- Caches attached to a resolved style, such as lazily computed pseudo-element styles, are not semantic mutation. They are fine as long as they are unobservable: anything reading the style gets the same answer whether or not the cache is populated.
- Invalidation classes (`AttributeChangeInvalidation` and friends) are RAII scopes around the mutation. The constructor sees the old state, the destructor the new one. Derived data keyed on the new state is dropped in the destructor.
- Anything holding pointers into resolver data must be cleared when the resolver goes away (`Scope::clearResolver()`).
- Per-sheet data belongs in the RuleSet, recorded by `RuleSetBuilder`. RuleSets are rebuilt after every stylesheet mutation, back pointers from rules to sheets are not.
- Interleaved layout for container queries sees a partially updated tree. Correctness comes from re-resolving after layout. Interleaving only saves work. Style can't predict whether a container's size will change.

## Caching

- Every input to a style must be either captured by the cache key and validation or make the resolution uncacheable. It must also be tracked for invalidation. `attr()` is the usual example.
- Cache keys are hints. Correctness comes from validating a candidate entry, so a collision costs work, never a wrong style.
- Fix cache thrashing in the cache's keying or eviction, not by threading flags through callers.
- Properties re-applied on cache hits must have no side effects.
- Element-keyed caches don't apply when evaluating in another context, such as custom function bodies.

## Values and substitution

- Parsed values, including `CSSSubstitutionValue`, are shared by every element a rule matches and across documents. Per-value caches must key on inputs held by `RefPtr`, never on `BuilderState`, registry or style data addresses, which get reused.

## Where code goes

- The subsystem separates generic mechanism (most of `TreeResolver`, `Builder`, `ElementRuleCollector`, the invalidation machinery, the caches) from narrow code specific to a property, selector or feature. Before changing a function, work out which kind it is. Don't put feature-specific cases into mechanism code. Route them through the existing extension points, in order of preference: `CSSProperties.json` codegen options and `Style::` value types, `BuilderState` hooks like `didApplyAuthorProperty()`, and custom builder functions as the escape hatch for properties with side effects or cross-property dependencies. If no extension point fits, add a narrow one.
- A painting or layout fix that needs to change `TreeResolver` is usually reconstructing knowledge that layer already has. Let it contribute that knowledge instead.

## Direction

- `ComputedStyle` holds computed values. Used values that depend on the box go through `UsedStyle` (`rendering/style/UsedStyle.h`), a rendering-layer view of a box, not through new `ComputedStyle::used*` accessors. Style-level comparisons use unzoomed `Style::effective*()` free functions.
- Computed values are zoom-independent and zoom is applied at use. Anything zoom- or device-scale-dependent belongs in `UsedStyle`.
- Text renderers have a full style of their own: inherited properties from the DOM parent, initial values for the rest.
- Per-value behavior lives on the `Style::` value types under `style/values/`: conversion from and to `CSSValue` (`+CSSValueConversion`, `+CSSValueCreation`), serialization (`+Serialization`) and blending (`+Blending`), driven by `CSSProperties.json`. `StyleBuilderCustom.h`, `StyleExtractorCustom.h` and `StyleInterpolationWrappers.h` are for genuine exceptions, not a pattern to copy.

## Testing

- Invalidation cost tests use `internals.styleInvalidationTraversalCount`. Tests using `internals` go in WebKit test directories, not WPT.
