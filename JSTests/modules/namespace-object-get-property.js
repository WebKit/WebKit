import { shouldThrow, shouldBe } from "./resources/assert.js";
import * as ns from "./namespace-object-get-property.js"

shouldThrow(() => {
    Reflect.get(ns, 'empty');
}, `ReferenceError: Cannot access uninitialized variable.`);
shouldBe(Reflect.get(ns, 'undefined'), undefined);

export let empty;
