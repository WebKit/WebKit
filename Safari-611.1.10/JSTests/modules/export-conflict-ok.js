import { A, B } from "./export-conflict-ok/main.js"  // C is conflict, but not looked up.
import { shouldBe } from "./resources/assert.js";

shouldBe(A, 42);
shouldBe(B, 50);
