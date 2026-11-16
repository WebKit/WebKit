import * as ns from "./module-namespace-is-frozen.js"
import {shouldThrow} from "./resources/assert.js"

shouldThrow(() => {
    Object.isFrozen(ns);
}, `ReferenceError: Cannot access 'a' before initialization.`);

export let a;
export function b () { }
