import { shouldBe } from "./resources/assert.js";
import * as root from "./namespace-single-walk/root.js";
import * as conflictOnly from "./namespace-single-walk/conflict-only.js";
import * as leafA from "./namespace-single-walk/leaf-a.js";

// (1) Unique Local binding via star: fast-path Resolved.
shouldBe(root.onlyA, "onlyA");
shouldBe(root.onlyB, "onlyB");

// (2) Same binding via two diamond paths (mid-left + mid-right both -> leaf-a): still single binding.
shouldBe(root.shared, "shared-from-a");
shouldBe(root.indirectTarget, "indirectTarget-a");

// (3) Root local shadows star-reachable conflict.
shouldBe(root.conflict, "conflict-root");
shouldBe("conflict" in root, true);

// (4) Without a root local, conflicting bindings are ambiguous and excluded from the namespace.
shouldBe("conflict" in conflictOnly, false);
shouldBe(conflictOnly.conflict, undefined);
shouldBe(conflictOnly.onlyA, "onlyA");
shouldBe(conflictOnly.onlyB, "onlyB");

// (5) Indirect re-export through star (slowPath) resolves to leaf-a's local.
shouldBe(root.indirect, "indirectTarget-a");
shouldBe(conflictOnly.indirect, "indirectTarget-a");

// (6) Namespace re-export (Type::Namespace) through star: fast-path Resolved.
shouldBe(root.nsOfA, leafA);
shouldBe(conflictOnly.nsOfA, leafA);

// Namespace key set is sorted and stable across both modules where the keys overlap.
shouldBe(Object.keys(root).join(","), "conflict,indirect,indirectTarget,nsOfA,onlyA,onlyB,shared");
shouldBe(Object.keys(conflictOnly).join(","), "indirect,indirectTarget,nsOfA,onlyA,onlyB,shared");

// (7) Mutual star cycle: visited set terminates the walk; both names visible from either side.
const cycleA = await import("./namespace-single-walk/cycle-a.js");
const cycleB = await import("./namespace-single-walk/cycle-b.js");
shouldBe(cycleA.fromA, "A");
shouldBe(cycleA.fromB, "B");
shouldBe(cycleB.fromA, "A");
shouldBe(cycleB.fromB, "B");
shouldBe(Object.keys(cycleA).join(","), "fromA,fromB");
shouldBe(Object.keys(cycleB).join(","), "fromA,fromB");
