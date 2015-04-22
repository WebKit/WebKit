function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, message) {
    var error = null;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("not thrown.");
    if (String(error) !== message)
        throw new Error("bad error: " + String(error));
}

var array = new Array(10);

for (var i = 0; i < 10; ++i) {
    (function (index) {
        var seenOnce = false;
        Object.defineProperty(array, index, {
            get() {
                if (seenOnce)
                    throw new Error('get is called.' + index);
                seenOnce = true;
                return index;
            }
        });
    }(i));
}

shouldBe(array.length, 10);

// Doesn't throw any errors.
var findValue = array.find(function (value) {
    return value === 9;
});
shouldBe(findValue, 9);

for (var i = 0; i < 10; ++i) {
    shouldThrow(function () {
        var value = array[i];
    }, "Error: get is called." + i);
}
