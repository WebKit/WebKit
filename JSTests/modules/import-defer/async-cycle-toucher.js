import defer * as ns from "./async-cycle-deferred.js";

// This module is evaluated while the cycle it belongs to is still on the evaluation stack, so
// GatherAsynchronousTransitiveDependencies finds nothing to await and the deferred graph stays
// unevaluated. Handing the access out lets the test poke it at a chosen point.
globalThis.asyncCycleTouch = () => ns.value;
globalThis.asyncCycleEvaluations.push("toucher");
