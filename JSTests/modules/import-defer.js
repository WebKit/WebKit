//@ requireOptions("--useImportDefer=1")
import { shouldBe } from "./resources/assert.js";

import defer * as ns1 from "./import-defer/dep.js";
import defer * as ns2 from "./import-defer/dep.js";
import * as nsEager from "./import-defer/dep.js";
import { reExportedDeferred } from "./import-defer/re-export.js";

shouldBe(ns1, ns2);
shouldBe(ns1, reExportedDeferred);
shouldBe(ns1 === nsEager, false);
shouldBe(Object.prototype.toString.call(ns1), "[object Deferred Module]");
shouldBe(Object.prototype.toString.call(nsEager), "[object Module]");

// "then" must not be visible on a deferred namespace and must not trigger evaluation.
shouldBe(ns1.then, undefined);
shouldBe(Reflect.has(ns1, "then"), false);
