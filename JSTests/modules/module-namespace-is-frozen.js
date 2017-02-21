import * as ns from "./module-namespace-is-frozen.js"
import {shouldThrow} from "./resources/assert.js"

shouldThrow(() => {
    Object.isFrozen(ns);
}, `ReferenceError: Cannot access uninitialized variable.`);

export let a;
export function b () { }
