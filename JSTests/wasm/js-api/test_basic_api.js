import * as assert from '../assert.js';
import * as utilities from '../utilities.js';

const version = 0x01;
const emptyModuleArray = Uint8Array.of(0x0, 0x61, 0x73, 0x6d, version, 0x00, 0x00, 0x00);
const invalidConstructorInputs = [undefined, null, "", 1, {}, []];
const invalidInstanceImports = [null, "", 1];

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
    "Module":       { typeofvalue: "function", writable: true, configurable: true, enumerable: false, length: 1 },
    "Instance":     { typeofvalue: "function", writable: true, configurable: true, enumerable: false, length: 1 },
    "Memory":       { typeofvalue: "function", writable: true, configurable: true, enumerable: false, length: 1 },
    "Table":        { typeofvalue: "function", writable: true, configurable: true, enumerable: false, length: 1 },
    "CompileError": { typeofvalue: "function", writable: true, configurable: true, enumerable: false, length: 1 },
    "LinkError":    { typeofvalue: "function", writable: true, configurable: true, enumerable: false, length: 1 },
    "RuntimeError": { typeofvalue: "function", writable: true, configurable: true, enumerable: false, length: 1 },
};


assert.isNotUndef(WebAssembly);
checkOwnPropertyDescriptor(utilities.global, "WebAssembly", { typeofvalue: "object", writable: true, configurable: true, enumerable: false });
assert.eq(String(WebAssembly), "[object WebAssembly]");
assert.isUndef(WebAssembly.length);
assert.eq(WebAssembly instanceof Object, true);
assert.throws(() => WebAssembly(), TypeError, `WebAssembly is not a function. (In 'WebAssembly()', 'WebAssembly' is an instance of WebAssembly)`);
assert.throws(() => new WebAssembly(), TypeError, `WebAssembly is not a constructor (evaluating 'new WebAssembly()')`);

for (const f in functionProperties) {
    assert.isNotUndef(WebAssembly[f]);
    assert.eq(WebAssembly[f].name, f);
    assert.eq(WebAssembly[f].length, functionProperties[f].length);
}

for (const c in constructorProperties) {
    assert.isNotUndef(WebAssembly[c]);
    assert.eq(WebAssembly[c].name, c);
    assert.eq(WebAssembly[c].length, constructorProperties[c].length);
    checkOwnPropertyDescriptor(WebAssembly, c, constructorProperties[c]);
    checkOwnPropertyDescriptor(WebAssembly[c], "prototype", { typeofvalue: "object", writable: false, configurable: false, enumerable: false });
    assert.throws(() => WebAssembly[c](), TypeError, `calling WebAssembly.${c} constructor without new is invalid`);
    switch (c) {
    case "Module":
        for (const invalid of invalidConstructorInputs)
            assert.throws(() => new WebAssembly[c](invalid), TypeError, `first argument must be an ArrayBufferView or an ArrayBuffer (evaluating 'new WebAssembly[c](invalid)')`);
        for (const buffer of [new ArrayBuffer(), new DataView(new ArrayBuffer()), new Int8Array(), new Uint8Array(), new Uint8ClampedArray(), new Int16Array(), new Uint16Array(), new Int32Array(), new Uint32Array(), new Float32Array(), new Float64Array()])
            // FIXME the following should be WebAssembly.CompileError. https://bugs.webkit.org/show_bug.cgi?id=163768
            assert.throws(() => new WebAssembly[c](buffer), Error, `WebAssembly.Module doesn't parse at byte 0 / 0: expected a module of at least 8 bytes (evaluating 'new WebAssembly[c](buffer)')`);
        assert.instanceof(new WebAssembly[c](emptyModuleArray), WebAssembly.Module);
        // FIXME test neutered TypedArray and TypedArrayView. https://bugs.webkit.org/show_bug.cgi?id=163899
        break;
    case "Instance":
        for (const invalid of invalidConstructorInputs)
            assert.throws(() => new WebAssembly[c](invalid), TypeError, `first argument to WebAssembly.Instance must be a WebAssembly.Module (evaluating 'new WebAssembly[c](invalid)')`);
        const instance = new WebAssembly[c](new WebAssembly.Module(emptyModuleArray));
        assert.instanceof(instance, WebAssembly.Instance);
        for (const invalid of invalidInstanceImports)
            assert.throws(() => new WebAssembly[c](new WebAssembly.Module(emptyModuleArray), invalid), TypeError, `second argument to WebAssembly.Instance must be undefined or an Object (evaluating 'new WebAssembly[c](new WebAssembly.Module(emptyModuleArray), invalid)')`);
        assert.isNotUndef(instance.exports);
        checkOwnPropertyDescriptor(instance, "exports", { typeofvalue: "object", writable: true, configurable: true, enumerable: true });
        // FIXME these should pass, requires a module namespace object. https://bugs.webkit.org/show_bug.cgi?id=165121
        // assert.isUndef(instance.exports.__proto__);
        // assert.eq(Reflect.isExtensible(instance.exports), false);
        // assert.eq(Symbol.iterator in instance.exports, true);
        // assert.eq(Symbol.toStringTag in instance.exports, true);
        break;
    case "Memory":
        new WebAssembly.Memory({initial: 20});
        break;
    case "Table":
        new WebAssembly.Table({initial: 20, element: "anyfunc"});
        new WebAssembly.Table({initial: 20, maximum: 20, element: "anyfunc"});
        new WebAssembly.Table({initial: 20, maximum: 25, element: "anyfunc"});
        break;
    case "CompileError":
    case "LinkError":
    case "RuntimeError": {
        const e = new WebAssembly[c];
        assert.eq(e instanceof WebAssembly[c], true);
        assert.eq(e instanceof Error, true);
        assert.eq(e instanceof TypeError, false);
        assert.eq(e.message, "");
        assert.eq(typeof e.stack, "string");
        const sillyString = "uh-oh!";
        const e2 = new WebAssembly[c](sillyString);
        // FIXME fix Compile / Runtime errors for this: assert.eq(e2.message, sillyString + " (evaluating 'new WebAssembly[c](sillyString)')");
    } break;
    default: throw new Error(`Implementation error: unexpected constructor property "${c}"`);
    }
}
