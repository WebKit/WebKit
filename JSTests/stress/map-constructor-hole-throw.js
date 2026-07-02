function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

var iterator;
[].values().__proto__.return = function () {
    iterator = this;
};

var array = [
    [0, 0],
    [1, 0],
    [2, 0],
];
Map.prototype.set = function () {
    throw new Error("ok");
};
shouldThrow(() => {
    var map = new Map(array);
}, `Error: ok`);
shouldBe(iterator !== undefined, true);
shouldBe(iterator.next().value, array[1]);

