import * as assert from '../assert.js';
import * as utilities from '../utilities.js';

const checkOwnPropertyDescriptor = (obj, prop, expect) => {
    const descriptor = Object.getOwnPropertyDescriptor(obj, prop);
    assert.eq(typeof descriptor.value, expect.typeofvalue);
    assert.eq(descriptor.writable, expect.writable);
    assert.eq(descriptor.configurable, expect.configurable);
    assert.eq(descriptor.enumerable, expect.enumerable);
};

const functionProperties = {
    "validate": { length: 1 },
    "compile":  { length: 1 },
};
const constructorProperties = {
    "Module":       { typeofvalue: "function", writable: true, configurable: true, enumerable: false, length: 1, isError: false },
    "Instance":     { typeofvalue: "function", writable: true, configurable: true, enumerable: false, length: 1, isError: false },
    "Memory":       { typeofvalue: "function", writable: true, configurable: true, enumerable: false, length: 1, isError: false },
    "Table":        { typeofvalue: "function", writable: true, configurable: true, enumerable: false, length: 1, isError: false },
    "CompileError": { typeofvalue: "function", writable: true, configurable: true, enumerable: false, length: 1, isError: true  },
    "RuntimeError": { typeofvalue: "function", writable: true, configurable: true, enumerable: false, length: 1, isError: true  },
};


assert.notUndef(WebAssembly);
checkOwnPropertyDescriptor(utilities.global, "WebAssembly", { typeofvalue: "object", writable: true, configurable: true, enumerable: false });
assert.eq(String(WebAssembly), "[object WebAssembly]");
assert.isUndef(WebAssembly.length);

for (const f in functionProperties) {
    assert.notUndef(WebAssembly[f]);
    assert.eq(WebAssembly[f].name, f);
    assert.eq(WebAssembly[f].length, functionProperties[f].length);
}

for (const c in constructorProperties) {
    assert.notUndef(WebAssembly[c]);
    assert.eq(WebAssembly[c].name, c);
    assert.eq(WebAssembly[c].length, constructorProperties[c].length);
    checkOwnPropertyDescriptor(WebAssembly, c, constructorProperties[c]);
    // Check the constructor's prototype.
    checkOwnPropertyDescriptor(WebAssembly[c], "prototype", { typeofvalue: "object", writable: false, configurable: false, enumerable: false });
    assert.eq(String(WebAssembly[c].prototype), `[object WebAssembly.${c}.prototype]`);
    assert.throws(() => WebAssembly[c](), TypeError, `calling WebAssembly.${c} constructor without new is invalid`);
}

// FIXME Implement and test these APIs further. For now they just throw. https://bugs.webkit.org/show_bug.cgi?id=159775

for (const f in functionProperties) {
    assert.throws(() => WebAssembly[f](), Error, `WebAssembly doesn't yet implement the ${f} function property`);
}

for (const c in constructorProperties)
    assert.throws(() => new WebAssembly[c](), Error, `WebAssembly doesn't yet implement the ${c} constructor property`);
