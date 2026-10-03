function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + ", expected: " + expected);
}

function keys(object) {
    var result = "";
    for (var key in object)
        result += key + ",";
    return result;
}
noInline(keys);

var factories = [
    function () { return function () { }; },
    function () { return () => { }; },
    function () { return function* () { }; },
    function () { return async function () { }; },
    function () { return { method() { } }.method; },
    function () { return function () { }.bind(null); },
];

for (var i = 0; i < testLoopCount; ++i) {
    var create = factories[i % factories.length];

    var namedFirst = create();
    namedFirst.a = 1;
    namedFirst[0] = 2;
    namedFirst[1] = 3;
    shouldBe(keys(namedFirst), "0,1,a,");
    shouldBe(keys(namedFirst), "0,1,a,");

    var indexedFirst = create();
    indexedFirst[0] = 1.5;
    indexedFirst[1] = 2.5;
    indexedFirst.a = 1;
    shouldBe(keys(indexedFirst), "0,1,a,");

    var indexedOnly = create();
    indexedOnly[0] = "a";
    shouldBe(keys(indexedOnly), "0,");

    var reified = create();
    reified[0] = 1;
    reified.a = 2;
    shouldBe(reified.length, 0);
    shouldBe(keys(reified), "0,a,");
}
