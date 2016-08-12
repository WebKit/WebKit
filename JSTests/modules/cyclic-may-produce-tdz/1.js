import { Cappuccino, Matcha } from "./2.js"
import { shouldThrow, shouldBe } from "../resources/assert.js";

export let Cocoa = "Cocoa";

// module "2" is not loaded yet, TDZ.
shouldThrow(() => {
    Cappuccino;
}, `ReferenceError: Cannot access uninitialized variable.`);

// But "Matcha" is variable (not lexical variable). It is already initialized as undefined.
shouldBe(Matcha, undefined);
