/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var names = [
    "isConcatSpreadable",
    "iterator",
    "match",
    "replace",
    "search",
    "species",
    "hasInstance",
    "split",
    "toPrimitive",
    "unscopables",
    "asyncIterator"
];

for (var name of names) {
    // Well-known symbols exist.
    assert.sameValue(typeof Symbol[name], "symbol");

    // They are never in the registry.
    assert.sameValue(Symbol[name] !== Symbol.for("Symbol." + name), true);

    // They are shared across realms.
    if (typeof Realm === 'function')
        throw new Error("please update this test to use Realms");
    if (typeof createNewGlobal === 'function') {
        var g = createNewGlobal();
        assert.sameValue(Symbol[name], g.Symbol[name]);
    }

    // Descriptor is all false.
    var desc = Object.getOwnPropertyDescriptor(Symbol, name);
    assert.sameValue(typeof desc.value, "symbol");
    assert.sameValue(desc.writable, false);
    assert.sameValue(desc.enumerable, false);
    assert.sameValue(desc.configurable, false);
}

