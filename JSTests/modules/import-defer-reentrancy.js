//@ requireOptions("--useImportDefer=1")
import { shouldBe } from "./resources/assert.js";

import defer * as ns from "./import-defer/reentrant.js";

// Trigger evaluation; reentrant.js will try to access its own deferred namespace while
// its [[Status]] is EVALUATING. ReadyForSyncExecution must return false -> TypeError.
shouldBe(ns.result instanceof TypeError, true);
