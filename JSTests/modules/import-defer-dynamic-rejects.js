//@ requireOptions("--useImportDefer=1")
import { shouldBe, shouldThrow } from "./resources/assert.js";

// A TLA-bearing transitive dependency that rejects must reject the
// import.defer() promise (the deferred root never executes).
let rejected;
try {
    await import.defer("./import-defer/cycle-a.js");
} catch (error) {
    rejected = error;
}
shouldBe(rejected.someError, "tla-reject");

// A deferred module that throws synchronously when accessed must NOT reject
// the import.defer() promise; the throw happens at access time.
const ns = await import.defer("./import-defer/throws.js");
shouldBe(Object.prototype.toString.call(ns), "[object Deferred Module]");
let thrown;
try {
    ns.value;
} catch (error) {
    thrown = error;
}
shouldBe(thrown.someError, "deferred-throw");
