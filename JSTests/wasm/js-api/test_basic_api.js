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
assert.eq(WebAssembly instanceof Object, true);
assert.throws(() => WebAssembly(), TypeError, `WebAssembly is not a function. (In 'WebAssembly()', 'WebAssembly' is an instance of WebAssembly)`);
assert.throws(() => new WebAssembly(), TypeError, `WebAssembly is not a constructor (evaluating 'new WebAssembly()')`);

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
    checkOwnPropertyDescriptor(WebAssembly[c], "prototype", { typeofvalue: "object", writable: false, configurable: false, enumerable: false });
    assert.throws(() => WebAssembly[c](), TypeError, `calling WebAssembly.${c} constructor without new is invalid`);
    if (constructorProperties[c].isError) {
        const e = new WebAssembly[c];
        assert.eq(e instanceof WebAssembly[c], true);
        assert.eq(e instanceof Error, true);
        assert.eq(e instanceof TypeError, false);
        assert.eq(e.message, "");
        assert.eq(typeof e.stack, "string");
        const sillyString = "uh-oh!";
        const e2 = new WebAssembly[c](sillyString);
        assert.eq(e2.message, sillyString);
    }
}

// FIXME Implement and test these APIs further. For now they just throw. https://bugs.webkit.org/show_bug.cgi?id=159775

for (const f in functionProperties) {
    assert.throws(() => WebAssembly[f](), Error, `WebAssembly doesn't yet implement the ${f} function property`);
}

for (const c in constructorProperties) {
    if (!constructorProperties[c].isError)
        assert.throws(() => new WebAssembly[c](), Error, `WebAssembly doesn't yet implement the ${c} constructor property`);
}
