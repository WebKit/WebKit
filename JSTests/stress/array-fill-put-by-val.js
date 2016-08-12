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
        var storage = null;
        Object.defineProperty(Array.prototype, index, {
            get() {
                throw new Error('get is called.' + index);
                return storage;
            },
            set(value) {
                if (seenOnce)
                    throw new Error('set is called.' + index);
                seenOnce = true;
                storage = value;
                return storage;
            }
        });
    }(i));
}

// No error, but all seenOnce becomes true.
array.fill(42);

// Ensures that all setter is called once.
for (var i = 0; i < 10; ++i) {
    shouldThrow(function () {
        array[i] = i;
    }, "Error: set is called." + i);
}
