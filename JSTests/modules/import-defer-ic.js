//@ requireOptions("--useImportDefer=1")
import { shouldBe } from "./resources/assert.js";

globalThis.deferEvaluations = [];

import defer * as ns from "./import-defer/eval-tracker.js";

// Warm up the property access through all JIT tiers without triggering evaluation.
function readSymbol(obj) { return obj[Symbol.toStringTag]; }
noInline(readSymbol);
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(readSymbol(ns), "Deferred Module");
shouldBe(globalThis.deferEvaluations.length, 0);

// First string-key access through the IC must still trigger evaluation exactly once.
function readValue(obj) { return obj.value; }
noInline(readValue);
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(readValue(ns), 42);
shouldBe(globalThis.deferEvaluations.length, 1);

// Once the deferred module is evaluated, the warmed-up IC must keep returning the
// same value while still bound to the same namespace constant.
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(readValue(ns), 42);
shouldBe(globalThis.deferEvaluations.length, 1);

// "then" must continue to be filtered out even after the IC warms up.
function readThen(obj) { return obj.then; }
noInline(readThen);
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(readThen(ns), undefined);
