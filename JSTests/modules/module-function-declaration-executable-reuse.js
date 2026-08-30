import { shouldBe } from "./resources/assert.js";
import * as A from "./module-function-declaration-executable-reuse/a.js";
import * as Same1 from "./module-function-declaration-executable-reuse/same-1.js";
import * as Same2 from "./module-function-declaration-executable-reuse/same-2.js";

shouldBe(A.f(), "f");
shouldBe(A.blockResult, "block");
shouldBe(A.gSeenInB, "g");
shouldBe(A.hSeenInB, "h");
shouldBe(A.kSeenInB, "k");
shouldBe(A.gInBody(), "replaced");
shouldBe(A.hInBody, 42);
shouldBe(A.kInBody, Math.max);
shouldBe(A.callCaptured(), "captured");
shouldBe(A.localResult, "local");
shouldBe(A.gen().next().value, "gen");
shouldBe(typeof A.asyncFn, "function");
shouldBe(typeof A.asyncGen, "function");

shouldBe(Same1.fInBody, Same2.f);
shouldBe(Same2.fInBody, Same1.f);
shouldBe(Same1.f(), "f");
shouldBe(Same2.f(), "f");
