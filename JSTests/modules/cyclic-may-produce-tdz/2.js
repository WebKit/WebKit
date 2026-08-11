import { Cocoa } from "./1.js"
import { shouldBe } from "../resources/assert.js";

export let Cappuccino = "Cappuccino";

export var Matcha = "Matcha";

// 1 is already loaded.
shouldBe(Cocoa, "Cocoa");

shouldBe(Cappuccino, "Cappuccino");
shouldBe(Matcha, "Matcha");

// Indirectly export "Cocoa"
export { Cocoa };
