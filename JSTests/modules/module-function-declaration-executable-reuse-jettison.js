//@ requireOptions("--forceCodeBlockToJettisonDueToOldAge=1", "--useEagerCodeBlockJettisonTiming=1")
import { shouldBe } from "./resources/assert.js";
import * as A from "./module-function-declaration-executable-reuse-jettison/a.js";

shouldBe(A.before, "f g h s");
shouldBe(A.after, "f 42 h2 s");
shouldBe(A.blockResult, "block");
shouldBe(A.fAfterBlock, "f");
