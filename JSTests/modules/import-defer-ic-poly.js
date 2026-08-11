//@ requireOptions("--useImportDefer=1")
import { shouldBe } from "./resources/assert.js";

globalThis.deferEvaluations = [];

import defer * as deferred from "./import-defer/eval-tracker.js";
import * as eager from "./import-defer/dep.js";

// Single accessor used polymorphically across a deferred namespace, an eager
// namespace, and plain objects. The ModuleNamespaceLoad IC is keyed on the specific
// namespace constant, so it must not confuse the receivers nor the deferred-vs-eager
// trigger semantics.
function readValue(obj) { return obj.value; }
noInline(readValue);

let plain = { value: 7 };
let plain2 = { other: 1, value: 8 };

// Warm up on the deferred namespace first; this triggers evaluation exactly once.
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(readValue(deferred), 42);
shouldBe(globalThis.deferEvaluations.length, 1);

// Now go polymorphic. Evaluation must not run again, and each receiver keeps its value.
for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(readValue(deferred), 42);
    shouldBe(readValue(eager), 1);
    shouldBe(readValue(plain), 7);
    shouldBe(readValue(plain2), 8);
}
shouldBe(globalThis.deferEvaluations.length, 1);
