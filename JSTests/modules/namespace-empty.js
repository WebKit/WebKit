import * as ns from "./namespace-empty.js"
import {shouldThrow} from "./resources/assert.js"

function access(ns)
{
    return ns.test;
}
noInline(access);

for (var i = 0; i < 1e3; ++i) {
    shouldThrow(() => {
        access(ns);
    }, `ReferenceError: Cannot access 'test' before initialization.`);
}


export let test = 42;
