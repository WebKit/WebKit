//@ requireOptions("--useImportDefer=1")
import { shouldBe, shouldThrow } from "./resources/assert.js";
import { blocker, aStarted } from "./import-defer/async-cycle-setup.js";

// SCC {tla, member}: tla has TLA and is the cycle root, member is the non-root member.
// While tla is suspended on the blocker, member has already reached EVALUATED even though the
// cycle has not finished. The deferred module behind ns depends on member, so it still cannot be
// evaluated synchronously: ReadyForSyncExecution must consult IsModuleSCCEvaluated(member), which
// follows [[CycleRoot]], rather than member's own [[Status]].

const evaluations = globalThis.asyncCycleEvaluations;

const pTLA = import("./import-defer/async-cycle-tla.js");
await aStarted.promise;

shouldBe(JSON.stringify(evaluations), JSON.stringify(["toucher", "member", "A-before-await"]));

shouldThrow(() => globalThis.asyncCycleTouch(), "TypeError: Unable to synchronously evaluate deferred module");
shouldBe(JSON.stringify(evaluations), JSON.stringify(["toucher", "member", "A-before-await"]));

blocker.resolve();
await pTLA;

// The cycle root is EVALUATED now, so the same access succeeds and evaluates only the deferred module.
shouldBe(globalThis.asyncCycleTouch(), 1);
shouldBe(JSON.stringify(evaluations), JSON.stringify(["toucher", "member", "A-before-await", "A-after-await", "deferred"]));
