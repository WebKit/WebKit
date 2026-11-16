//@ requireOptions("--useImportDefer=1")
import { shouldBe } from "./resources/assert.js";

import defer * as ns from "./import-defer/tla-parent.js";

// GatherAsynchronousTransitiveDependencies must have eagerly evaluated the TLA-bearing
// dependency (and its sync deps) on the parent's async pipeline; the synchronous bits
// behind the defer (tla-parent itself, sync-dep) remain unevaluated.
shouldBe(JSON.stringify(globalThis.deferTLAEvaluations), JSON.stringify(["tla-dep-of-tla", "tla-child"]));

// Touching the namespace synchronously evaluates the rest.
shouldBe(ns.value, 1);
shouldBe(JSON.stringify(globalThis.deferTLAEvaluations), JSON.stringify(["tla-dep-of-tla", "tla-child", "sync-dep", "tla-parent"]));
