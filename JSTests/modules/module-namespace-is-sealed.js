import * as ns from "./module-namespace-is-sealed.js"
import {shouldThrow} from "./resources/assert.js"

shouldThrow(() => {
    Object.isSealed(ns);
}, `ReferenceError: Cannot access 'a' before initialization.`);

export let a;
export function b () { }
