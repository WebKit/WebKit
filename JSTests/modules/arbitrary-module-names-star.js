import { "*" as star } from "./arbitrary-module-names-star/export.js";
import { "Infinity" as inf } from "./arbitrary-module-names-star/export.js";
import { "default" as def } from "./arbitrary-module-names-star/export.js";
import { "" as empty } from "./arbitrary-module-names-star/export.js";
import * as ns from "./arbitrary-module-names-star/export.js";
import { shouldBe } from "./resources/assert.js";

// "*" as a ModuleExportName must be treated as a single named binding,
// not as a NamespaceImport.
shouldBe(star, "ok");
shouldBe(inf, "ok");
shouldBe(def, "ok");
shouldBe(empty, "ok");

shouldBe(typeof ns, "object");
shouldBe(ns["*"], "ok");
shouldBe(ns["Infinity"], "ok");
shouldBe(ns["default"], "ok");
shouldBe(ns[""], "ok");
