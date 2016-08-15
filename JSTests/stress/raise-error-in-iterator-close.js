
function createIterator(callback) {
    var array = [0,1,2,3,4,5];
    var iterator = array[Symbol.iterator]();
    iterator.return = function () {
        iterator.returned = true;
        if (callback)
            return callback(this);
        return { done: true, value: undefined };
    };
    iterator.returned = false;
    return iterator;
}

(function test() {
    var outerIterator = createIterator();
    var innerIterator = createIterator(function () {
        throw new Error("Inner return called.");
    });
    var error = null;
    try {
        outer: for (var e1 of outerIterator) {
            inner: for (var e2 of innerIterator) {
                break;
            }
        }
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("no error");
    if (String(error) !== "Error: Inner return called.")
        throw new Error("bad error: " + String(error));
    if (!innerIterator.returned)
        throw new Error("bad value: " + innerIterator.returned);
    if (!outerIterator.returned)
        throw new Error("bad value: " + outerIterator.returned);
}());

(function test() {
    var outerIterator = createIterator(function () {
        throw new Error("Outer return called.");
    });
    var innerIterator = createIterator(function () {
        throw new Error("Inner return called.");
    });
    var error = null;
    try {
        outer: for (var e1 of outerIterator) {
            inner: for (var e2 of innerIterator) {
                break;
            }
        }
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("no error");
    if (String(error) !== "Error: Inner return called.")
        throw new Error("bad error: " + String(error));
    if (!innerIterator.returned)
        throw new Error("bad value: " + innerIterator.returned);
    if (!outerIterator.returned)
        throw new Error("bad value: " + outerIterator.returned);
}());

(function test() {
    var outerIterator = createIterator(function () {
        throw new Error("Outer return called.");
    });
    var innerIterator = createIterator();
    var error = null;
    try {
        outer: for (var e1 of outerIterator) {
            inner: for (var e2 of innerIterator) {
                break outer;
            }
        }
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("no error");
    if (String(error) !== "Error: Outer return called.")
        throw new Error("bad error: " + String(error));
    if (!innerIterator.returned)
        throw new Error("bad value: " + innerIterator.returned);
    if (!outerIterator.returned)
        throw new Error("bad value: " + outerIterator.returned);
}());

(function test() {
    var outerIterator = createIterator(function () {
        throw new Error("Outer return called.");
    });
    var innerIterator = createIterator(function () {
        throw new Error("Inner return called.");
    });
    var error = null;
    try {
        outer: for (var e1 of outerIterator) {
            inner: for (var e2 of innerIterator) {
                throw new Error("Loop raises error.");
            }
        }
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("no error");
    if (String(error) !== "Error: Loop raises error.")
        throw new Error("bad error: " + String(error));
    if (!innerIterator.returned)
        throw new Error("bad value: " + innerIterator.returned);
    if (!outerIterator.returned)
        throw new Error("bad value: " + outerIterator.returned);
}());

