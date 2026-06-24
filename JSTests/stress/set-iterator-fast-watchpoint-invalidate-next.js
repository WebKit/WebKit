// Replacing SetIterator.prototype.next fires setIteratorProtocolWatchpointSet.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}

function values(set) {
    var sum = 0;
    for (var v of set.values())
        sum += v;
    return sum;
}
noInline(values);

function plain(set) {
    var sum = 0;
    for (var v of set)
        sum += v;
    return sum;
}
noInline(plain);

var set = new Set();
for (var i = 0; i < 10; ++i)
    set.add(i + 1);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(values(set), 55);
    shouldBe(plain(set), 55);
}

// Replace SetIterator.prototype.next — fires the watchpoint.
var setIteratorPrototype = Object.getPrototypeOf((new Set()).values());
var counter = 0;
setIteratorPrototype.next = function () {
    counter++;
    return counter <= 3 ? { value: 7, done: false } : { value: undefined, done: true };
};

counter = 0;
shouldBe(values(set), 21);
counter = 0;
shouldBe(plain(set), 21);
