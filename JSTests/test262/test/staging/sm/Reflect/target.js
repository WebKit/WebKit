/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-Reflect-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
// Check correct handling of the `target` argument shared by every Reflect method.

// For each standard Reflect method, an array of arguments
// that would be OK after a suitable target argument.
var methodInfo = {
    apply: [undefined, []],
    construct: [[]],
    defineProperty: ["x", {}],
    deleteProperty: ["x"],
    get: ["x", {}],
    getOwnPropertyDescriptor: ["x"],
    getPrototypeOf: [],
    has: ["x"],
    isExtensible: [],
    ownKeys: [],
    preventExtensions: [],
    set: ["x", 0],
    setPrototypeOf: [{}]
};

// Check that all Reflect properties are listed above.
for (const name of Reflect.ownKeys(Reflect)) {
    // If this assertion fails, congratulations on implementing a new Reflect feature!
    // Add it to methodInfo above.
    if (typeof name !== "symbol" && name !== "parse")
      assert.sameValue(name in methodInfo, true);
}

for (const name of Object.keys(methodInfo)) {
    var args = methodInfo[name];

    // The target argument is required.
    assertThrowsInstanceOf(Reflect[name], TypeError);

    // Throw if the target argument is not an object.
    for (var value of SOME_PRIMITIVE_VALUES) {
        assertThrowsInstanceOf(() => Reflect[name](value, ...args), TypeError);
    }
}

