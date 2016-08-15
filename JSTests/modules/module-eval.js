import { Cappuccino, Cocoa } from "./module-eval/drink.js"
import * as B from "./module-eval/B.js"
import { shouldThrow, shouldBe } from "./resources/assert.js"

shouldBe(eval("Cappuccino"), "Cappuccino");

(function () {
    let Cappuccino = "Cocoa";
    shouldBe(eval("Cappuccino"), "Cocoa");
    shouldBe(eval("Cocoa"), "Cocoa");
}());
