//@ requireOptions("--useImportDefer=1")
import { shouldBe } from "./resources/assert.js";
import defer * as nsStatic from "./import-defer/dep.js";

const ns = await import.defer("./import-defer/dep.js");

// Dynamic and static defer must hand back the same deferred namespace identity.
shouldBe(ns, nsStatic);
shouldBe(Object.prototype.toString.call(ns), "[object Deferred Module]");

// Deferred namespaces hide "then" so the resolution algorithm doesn't unwrap them.
shouldBe(ns.then, undefined);
shouldBe(Reflect.has(ns, "then"), false);

// Touching the namespace evaluates the module synchronously.
shouldBe(ns.value, 1);
// "then" stays hidden on the deferred namespace even after evaluation.
shouldBe(ns.then, undefined);
