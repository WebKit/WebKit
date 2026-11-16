//@ requireOptions("--useImportDefer=1")
import { shouldBe } from "./resources/assert.js";

globalThis.deferEvaluations = [];

// A graph with no asynchronous transitive dependencies must not run any module
// body before the import.defer() promise resolves.
const ns = await import.defer("./import-defer/eval-tracker.js");
shouldBe(JSON.stringify(globalThis.deferEvaluations), "[]");

// A symbol-keyed lookup must not trigger evaluation either.
shouldBe(ns[Symbol.toStringTag], "Deferred Module");
shouldBe(JSON.stringify(globalThis.deferEvaluations), "[]");

// First non-symbol access evaluates the module synchronously.
shouldBe(ns.value, 42);
shouldBe(JSON.stringify(globalThis.deferEvaluations), JSON.stringify(["eval-tracker"]));

// Subsequent import.defer() calls hand back the same namespace and never re-evaluate.
const ns2 = await import.defer("./import-defer/eval-tracker.js");
shouldBe(ns2, ns);
shouldBe(JSON.stringify(globalThis.deferEvaluations), JSON.stringify(["eval-tracker"]));
