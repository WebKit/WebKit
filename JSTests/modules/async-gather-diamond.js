// Diamond fan-in through GatherAvailableAncestors:
//
//   leaf (TLA)
//   /        \
//  A          B           (sync sibling re-exporters)
//   \        /
//    \      /
//      C                  (sync, depends on both A and B)
//
// When leaf fulfills, GatherAvailableAncestors walks leaf's
// AsyncParentModules ([A, B]). For each, it appends to execList and recurses
// into its sync parents (only C). C is reachable via two paths (leaf -> A -> C
// and leaf -> B -> C) within a single gather call, so C must be appended
// exactly once. This exercises the membership-test path that the
// "PendingAsyncDependencies != 0" optimization replaces; if the equivalence
// were broken, C would either be appended twice (executing its body twice)
// or be skipped entirely.

import { shouldBe } from "./resources/assert.js";
import { c } from "./async-gather-diamond/C.js";
import { log } from "./async-gather-diamond/log.js";

shouldBe(c, 2);
shouldBe(log.join(","), "A,B,C");
