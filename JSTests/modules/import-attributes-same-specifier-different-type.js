//@ requireOptions("--useImportDefer=1")
import { shouldBe } from "./resources/assert.js";

// The same specifier is requested both as a JSON module (evaluation phase) and as a
// JavaScript module (defer phase), so [[LoadedModules]] holds it under both types.
// Each binding must resolve against the module record matching its request's type.
import jsonValue from "./resources/dual-type.js" with { type: "json" };
import * as jsonNs from "./resources/dual-type.js" with { type: "json" };
import defer * as deferredNs from "./resources/dual-type.js";
import { reExportedJson, reExportedJsonNs, deferredTag } from "./resources/dual-type-reexport.js";

shouldBe(jsonValue, 42);
shouldBe(jsonNs.default, 42);
shouldBe(Object.prototype.toString.call(deferredNs), "[object Deferred Module]");
shouldBe(reExportedJson, 42);
shouldBe(reExportedJsonNs.default, 42);
shouldBe(deferredTag, "[object Deferred Module]");
