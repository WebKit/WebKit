// Replacing ArrayIteratorPrototype.next fires arrayIteratorProtocolWatchpointSet.
// Functions warmed up under the fast path must keep producing correct results.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}

function values(array) {
    var sum = 0;
    for (var v of array.values())
        sum += v;
    return sum;
}
noInline(values);

function plain(array) {
    var sum = 0;
    for (var v of array)
        sum += v;
    return sum;
}
noInline(plain);

var array = [];
for (var i = 0; i < 10; ++i)
    array.push(i + 1);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(values(array), 55);
    shouldBe(plain(array), 55);
}

// Replace ArrayIteratorPrototype.next — fires the watchpoint.
var arrayIteratorPrototype = Object.getPrototypeOf([].values());
arrayIteratorPrototype.next = function () {
    return { value: 7, done: this._done };
};
arrayIteratorPrototype._done = false;

// One call to .next() returns 7, second sets _done=true via outer code below.
// Use a very small array so we don't infinite-loop here — set _done after one call.
var counter = 0;
arrayIteratorPrototype.next = function () {
    counter++;
    return counter <= 3 ? { value: 7, done: false } : { value: undefined, done: true };
};

counter = 0;
shouldBe(values(array), 21); // 7 + 7 + 7
counter = 0;
shouldBe(plain(array), 21);
