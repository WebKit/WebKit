import { shouldThrow, shouldBe } from "./resources/assert.js";
import * as ns from "./namespace-object-get-property.js"

shouldThrow(() => {
    Reflect.get(ns, 'empty');
}, `ReferenceError: Cannot access 'empty' before initialization.`);
shouldBe(Reflect.get(ns, 'undefined'), undefined);

export let empty;
