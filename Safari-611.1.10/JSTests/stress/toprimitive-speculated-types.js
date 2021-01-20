function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + JSON.stringify(actual));
}

function raw(array) {
    var result = '';
    for (var i = 0; i < array.length; ++i) {
        result += array[i];
    }
    return result;
}

function Counter() {
    return {
        count: 0,
        toString() {
            // Return a number even if the "toString" method.
            return this.count++;
        }
    };
}

for (var i = 0; i < 10000; ++i) {
    var c = Counter();
    shouldBe(raw([c, c]), "01");
}
