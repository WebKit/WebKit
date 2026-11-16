//@ requireOptions("--useImportDefer=1")
import { shouldBe } from "./resources/assert.js";

globalThis.deferEvaluations = [];

import defer * as ns from "./import-defer/eval-tracker.js";

shouldBe(globalThis.deferEvaluations.length, 0);

// Symbol/`then` accesses must not trigger evaluation.
ns[Symbol.toStringTag];
ns.then;
shouldBe(globalThis.deferEvaluations.length, 0);

// First string access triggers synchronous evaluation.
shouldBe(ns.value, 42);
shouldBe(globalThis.deferEvaluations.length, 1);
shouldBe(globalThis.deferEvaluations[0], "eval-tracker");

// Subsequent accesses do not re-evaluate.
shouldBe(ns.value, 42);
Reflect.ownKeys(ns);
shouldBe(globalThis.deferEvaluations.length, 1);
