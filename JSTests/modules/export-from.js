import * as namespace from "./export-from/main.js"
import { shouldBe } from "./resources/assert.js";

shouldBe(JSON.stringify(Object.getOwnPropertyNames(namespace)), `["Cappuccino","Cocoa","default","enum"]`);
shouldBe(namespace.Cocoa, "Cocoa");
shouldBe(namespace.Cappuccino, "Cappuccino");
shouldBe(namespace.default, "Mocha");
shouldBe(namespace.enum, "Matcha");
