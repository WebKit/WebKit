import Builder from '../Builder.js';
import * as assert from '../assert.js';

const iterations = 32;

// This API isn't part of WebAssembly's official spec. It is use for testing within the shell.

const version = 0x01;
const emptyModuleArray = Uint8Array.of(0x0, 0x61, 0x73, 0x6d, version, 0x00, 0x00, 0x00);

assert.isFunction(WebAssemblyMemoryMode);

const message = `WebAssemblyMemoryMode expects either a WebAssembly.Memory or WebAssembly.Instance`;
assert.throws(() => WebAssemblyMemoryMode(null), TypeError, message);
assert.throws(() => WebAssemblyMemoryMode(undefined), TypeError, message);
assert.throws(() => WebAssemblyMemoryMode(1), TypeError, message);
assert.throws(() => WebAssemblyMemoryMode(""), TypeError, message);
assert.throws(() => WebAssemblyMemoryMode({}), TypeError, message);
assert.throws(() => WebAssemblyMemoryMode(new WebAssembly.Module(emptyModuleArray)), TypeError, message);
assert.throws(() => WebAssemblyMemoryMode(new WebAssembly.Table({initial: 1, element: "anyfunc"})), TypeError, message);

const validateMode = what => {
    const mode = WebAssemblyMemoryMode(what);
    switch (mode) {
    case "Signaling":
        break;
    case "BoundsChecking":
        break;
    default:
        throw new Error(`Unexpected WebAssembly.Memory mode '${mode}'`);
    }
    return what;
}

const instantiate = builder => {
    const bin = builder.WebAssembly();
    const module = new WebAssembly.Module(bin.get());
    return new WebAssembly.Instance(module);
};

(function testMemoryNoMax() {
    let memories = [];
    for (let i = 0; i != iterations; ++i)
        memories.push(validateMode(new WebAssembly.Memory({ initial: i })));
    return memories;
})();

fullGC();

(function testMemory() {
    let memories = [];
    for (let i = 0; i != iterations; ++i)
        memories.push(validateMode(new WebAssembly.Memory({ initial: i, maximum: i })));
    return memories;
})();

fullGC();

(function testInstanceNoMemory() {
    let instances = [];
    for (let i = 0; i != iterations; ++i) {
        const builder = (new Builder())
              .Type().End()
              .Function().End()
              .Code().End();
        const instance = instantiate(builder);
        // No-memory instances should never be Signaling: it would be wasteful.
        assert.eq(WebAssemblyMemoryMode(instance), "BoundsChecking");
        instances.push(instance);
    }
    return instances;
})();

fullGC();

(function testInstanceNoMax() {
    let instances = [];
    for (let i = 0; i != iterations; ++i) {
        // Note: not exported! The internal API can still access.
        const builder = (new Builder())
              .Type().End()
              .Function().End()
              .Memory().InitialMaxPages(i).End()
              .Code().End();
        instances.push(validateMode(instantiate(builder)));
    }
})();

fullGC();

(function testInstance() {
    let instances = [];
    for (let i = 0; i != iterations; ++i) {
        // Note: not exported! The internal API can still access.
        const builder = (new Builder())
              .Type().End()
              .Function().End()
              .Memory().InitialMaxPages(i, i).End()
              .Code().End();
        instances.push(validateMode(instantiate(builder)));
    }
})();
