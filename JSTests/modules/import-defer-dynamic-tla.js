//@ requireOptions("--useImportDefer=1")
import { shouldBe } from "./resources/assert.js";

// import.defer() of a graph with TLA must eagerly evaluate the TLA-bearing
// dependency (and its sync deps) before the promise resolves, but the deferred
// root and the rest of the synchronous chain stay unevaluated.
const ns = await import.defer("./import-defer/tla-parent.js");
shouldBe(JSON.stringify(globalThis.deferTLAEvaluations), JSON.stringify(["tla-dep-of-tla", "tla-child"]));
shouldBe(Object.prototype.toString.call(ns), "[object Deferred Module]");

// Touching the namespace evaluates the rest synchronously.
shouldBe(ns.value, 1);
shouldBe(JSON.stringify(globalThis.deferTLAEvaluations), JSON.stringify(["tla-dep-of-tla", "tla-child", "sync-dep", "tla-parent"]));
