import { blocker, aStarted } from "./async-cycle-setup.js";
import "./async-cycle-member.js";

globalThis.asyncCycleEvaluations.push("A-before-await");
aStarted.resolve();
await blocker.promise;
globalThis.asyncCycleEvaluations.push("A-after-await");
