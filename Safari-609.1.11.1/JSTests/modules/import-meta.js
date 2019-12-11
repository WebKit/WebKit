import { shouldBe, shouldNotBe } from "./resources/assert.js";
import { cocoa } from "./import-meta/cocoa.js"

shouldNotBe(cocoa, import.meta);
shouldBe(typeof cocoa, "object");
shouldBe(typeof import.meta, "object");
shouldBe(import.meta.filename.endsWith("import-meta.js"), true);
shouldBe(cocoa.filename.endsWith("cocoa.js"), true);

shouldBe(Reflect.getPrototypeOf(cocoa), null);
shouldBe(Reflect.getPrototypeOf(import.meta), null);
