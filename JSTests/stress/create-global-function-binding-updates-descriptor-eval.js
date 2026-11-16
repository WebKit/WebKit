function assert(x) {
    if (!x)
        throw new Error("Bad assertion!");
}

function stringifyDescriptor(key) {
    return JSON.stringify(Object.getOwnPropertyDescriptor(globalThis, key), (_, v) => typeof v === "function" ? "<function>" : v, 2);
}

Object.defineProperties(globalThis, {
    conf1: { value: {}, writable: true, enumerable: true, configurable: true },
    conf2: { value: {}, writable: true, enumerable: false, configurable: true },
    conf3: { value: {}, writable: false, enumerable: false, configurable: true },
    conf4: { get() {}, set: undefined, enumerable: true, configurable: true },
    conf5: { get() {}, set() {}, enumerable: false, configurable: true },
    nonConf: { value: {}, writable: true, enumerable: true, configurable: false },
});

eval(`
    function conf1() {}
    function conf2() {}
    function conf3() {}
    function conf4() {}
    function conf5() {}
    function nonConf() {}
`);

const expectedConfigurableDescriptor = `{
  "value": "<function>",
  "writable": true,
  "enumerable": true,
  "configurable": true
}`;

const expectedNonConfigurableDescriptor = `{
  "value": "<function>",
  "writable": true,
  "enumerable": true,
  "configurable": false
}`;

assert(stringifyDescriptor("conf1") === expectedConfigurableDescriptor);
assert(stringifyDescriptor("conf2") === expectedConfigurableDescriptor);
assert(stringifyDescriptor("conf3") === expectedConfigurableDescriptor);
assert(stringifyDescriptor("conf4") === expectedConfigurableDescriptor);
assert(stringifyDescriptor("conf5") === expectedConfigurableDescriptor);
assert(stringifyDescriptor("nonConf") === expectedNonConfigurableDescriptor);
