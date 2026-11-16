# WebCore/css

Architectural rules that aren't visible from any single file. Read together with `../style/AGENTS.md`.

The bar for changing this file is high. Entries state rules, invariants and direction, not descriptions of current code. If a patch makes an entry inaccurate, fix or delete it in the same patch.

## css/ vs style/

- `css/` owns syntax: tokenizing, parsing, specified values, serialization, selectors, rule and sheet data structures, and the CSSOM. `style/` owns everything that depends on an element, the tree or document state: cascade, computed values, invalidation. Code in `css/` that needs an element, a `BuilderState` or a `ComputedStyle` probably belongs in `style/`.
- Specified values are `CSS::` types under `css/values/`. Computed values are `Style::` types under `style/values/`, organized by the same spec modules. Anything that needs layout is resolved at used-value time, never in the parser or in conversion to `Style::` types.
- Style resolution and layout treat `css/` objects as read-only input. Semantic mutation comes from parsing and the CSSOM only. Known exceptions, not to be extended: lazy reference resolution in `CSSCounterStyleRegistry`, and `@font-face` loads started by `CSSFontFace` during text measurement.
- Caches on `css/` objects, such as the substitution cache on `CSSSubstitutionValue`, are fine if they are unobservable: results are identical whatever the cache state, and the sharing rules under Values hold.
- As in `style/`, there is generic mechanism (tokenizer, parser infrastructure, calc, value type machinery, codegen) and code specific to a property or feature. Don't thread feature logic into shared machinery. A helper in a shared base that only one client uses belongs with that client.

## Values

- `CSSValue` subclasses are the legacy representation. New values are plain `CSS::` structs wired with `DEFINE_TYPE_MAPPING`, with a thin `CSSValue` wrapper only where one is still needed. Files named `Deprecated*` are legacy paths.
- Parsed values and declaration blocks are immutable and shared by every element a rule matches and across documents. A cache on a value must validate by data identity and hold `RefPtr`s, never key on element, document or registry addresses. Nothing reachable from a value may point back to elements or declaration blocks.
- The `Style::` calc tree has no unresolved leaves. Element-dependent functions are resolved during conversion.

## Parsing

- Gate disabled features at parse time (`CSSParserContext`, `MediaQueryParser::schemaForFeatureName()`) so they are undetectable from script.
- `CSSParserContext` is part of global cache keys (`StyleSheetContentsCache` keys on the sheet text and the context). The same context and string must always parse to the same result. Only fields that affect the parse result belong in it. Per-parse state goes in parser state such as `CSS::PropertyParserState`.
- Keep grammars as generic as the spec. Don't vary a grammar by name or by setting when the spec defines one.
- When a generic mechanism replaces a legacy grammar, delete the legacy one. Property grammars are tried before substitution, so a leftover legacy grammar shadows the generic path.
- Derive predicates about substitution functions from their dispatch, not from separately maintained lists.

## Rules and sheets

- `StyleSheetContents` is shared between sheets and copied on write by CSSOM mutation. Back pointers from rules to their sheet go stale. Per-sheet data belongs in the RuleSet built in `style/`.

## Codegen

- Prefer a `CSSProperties.json` codegen key over hand-written per-property code.
