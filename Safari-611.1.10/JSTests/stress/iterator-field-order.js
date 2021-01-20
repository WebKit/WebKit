function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array = [ 42 ];

shouldBe(JSON.stringify(array.values().next()), `{"value":42,"done":false}`);
shouldBe(JSON.stringify(array.keys().next()), `{"value":0,"done":false}`);
shouldBe(JSON.stringify(array.entries().next()), `{"value":[0,42],"done":false}`);

async function* asyncIterator() {
    yield 42;
}

var iterator = asyncIterator();
iterator.next().then(function (value) {
    shouldBe(JSON.stringify(value), `{"value":42,"done":false}`);
}).catch($vm.abort);

function* generator() {
    yield 42;
}

shouldBe(JSON.stringify(generator().next()), `{"value":42,"done":false}`);

var map = new Map([[0,42]]);
shouldBe(JSON.stringify(map.keys().next()), `{"value":0,"done":false}`);
shouldBe(JSON.stringify(map.values().next()), `{"value":42,"done":false}`);
shouldBe(JSON.stringify(map.entries().next()), `{"value":[0,42],"done":false}`);

var set = new Set([42]);
shouldBe(JSON.stringify(set.keys().next()), `{"value":42,"done":false}`);
shouldBe(JSON.stringify(set.values().next()), `{"value":42,"done":false}`);
shouldBe(JSON.stringify(set.entries().next()), `{"value":[42,42],"done":false}`);

var string = "Cocoa";
shouldBe(JSON.stringify(string[Symbol.iterator]().next()), `{"value":"C","done":false}`);
