import {
    Cocoa,
    Cappuccino,
    Matcha,
    Modifier
} from "./imported-bindings-can-be-changed-in-original-module/bindings.js"
import { shouldBe, shouldThrow } from "./resources/assert.js"

shouldBe(Cocoa, "Cocoa");
shouldBe(Cappuccino, "Cappuccino");
shouldBe(Matcha, "Matcha");

Modifier.change("Cocoa");

shouldBe(Cocoa, "Cocoa");
shouldBe(Cappuccino, "Cocoa");
shouldBe(Matcha, "Cocoa");
