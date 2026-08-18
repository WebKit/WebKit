function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}

function testFast(o, expected) {
    return Object.isExtensible(o);
}
noInline(testFast);

function testGeneric(v, expected) {
    return Object.isExtensible(v);
}
noInline(testGeneric);

function makeArguments() {
    return arguments;
}

var cases = [
    { make: () => makeArguments(1, 2, 3), name: "DirectArguments" },
    { make: () => (function() { "use strict"; return arguments; })(1, 2, 3), name: "ScopedArguments" },
    { make: () => [1, 2, 3], name: "Array" },
    { make: () => new ArrayBuffer(8), name: "ArrayBuffer" },
    { make: () => new Int32Array(4), name: "Int32Array" },
    { make: () => new Float64Array(4), name: "Float64Array" },
    { make: () => new DataView(new ArrayBuffer(8)), name: "DataView" },
    { make: () => /abc/, name: "RegExp" },
    { make: () => new Date(), name: "Date" },
];

for (var testCase of cases) {
    var extensible = testCase.make();
    var nonExtensible = Object.preventExtensions(testCase.make());

    for (var i = 0; i < 20000; ++i) {
        assert(testFast(extensible) === true);
        assert(testFast(nonExtensible) === false);
        assert(testGeneric(extensible) === true);
        assert(testGeneric(nonExtensible) === false);
    }
}

// Mixed/polymorphic call sites, to also cover the megamorphic feedback case.
(function() {
    var extensibleObjs = cases.map(c => c.make());
    var nonExtensibleObjs = cases.map(c => Object.preventExtensions(c.make()));

    for (var i = 0; i < 20000; ++i) {
        var idx = i % cases.length;
        assert(testGeneric(extensibleObjs[idx]) === true);
        assert(testGeneric(nonExtensibleObjs[idx]) === false);
    }
})();
