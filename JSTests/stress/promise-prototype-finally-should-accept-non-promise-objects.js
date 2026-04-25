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

class Thenable {
    constructor(promise)
    {
        this.promise = promise;
    }

    then(fulfill, rejected)
    {
        return new Thenable(this.promise.then(fulfill, rejected));
    }
};

var fulfilled = new Thenable(Promise.reject(0));
var rejected = new Thenable(Promise.resolve(42));

var counter = 0;
Promise.prototype.finally.call(fulfilled, function () { counter++; });
Promise.prototype.finally.call(rejected, function () { counter++; });
drainMicrotasks();
shouldBe(counter, 2);

shouldThrow(() => Promise.prototype.finally.call(32), `TypeError: |this| is not an object`);
