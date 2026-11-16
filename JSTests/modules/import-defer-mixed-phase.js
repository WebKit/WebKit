//@ requireOptions("--useImportDefer=1")
import { shouldBe } from "./resources/assert.js";

// Same specifier requested in both defer and evaluation phases. Per spec they are
// separate ModuleRequest records; the evaluation-phase entry must drive evaluation
// at its position in source order, after the eager middle dependency.
import defer * as nsDefer from "./import-defer/mixed-dep.js";
import "./import-defer/mixed-middle.js";
import * as nsEager from "./import-defer/mixed-dep.js";

shouldBe(JSON.stringify(globalThis.deferMixedEvaluations), JSON.stringify(["middle", "dep"]));
shouldBe(nsDefer === nsEager, false);
shouldBe(nsDefer.value, nsEager.value);
shouldBe(globalThis.deferMixedEvaluations.length, 2);
