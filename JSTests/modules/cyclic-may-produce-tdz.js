import { Cocoa, Cappuccino, Matcha } from "./cyclic-may-produce-tdz/2.js"
import { shouldBe } from "./resources/assert.js";

// All things are already set.
shouldBe(Cocoa, "Cocoa");
shouldBe(Cappuccino, "Cappuccino");
shouldBe(Matcha, "Matcha");
