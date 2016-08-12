
function createIterator(callback) {
    var array = [0,1,2,3,4,5];
    var iterator = array[Symbol.iterator]();
    iterator.return = function () {
        ++iterator.returned;
        if (callback)
            return callback(this);
        return { done: true, value: undefined };
    };
    iterator.returned = 0;
    return iterator;
}

(function test() {
    var outerIterator = createIterator();
    var innerIterator = createIterator();
    outer: for (var e1 of outerIterator) {
        inner: for (var e2 of innerIterator) {
            break outer;
        }
    }
    if (innerIterator.returned !== 1)
        throw new Error("bad value: " + innerIterator.returned);
    if (outerIterator.returned !== 1)
        throw new Error("bad value: " + outerIterator.returned);
}());

(function test() {
    var outerIterator = createIterator();
    var innerIterator = createIterator();
    outer: for (var e1 of outerIterator) {
        inner: for (var e2 of innerIterator) {
            break inner;
        }
    }
    if (innerIterator.returned !== 6)
        throw new Error("bad value: " + innerIterator.returned);
    if (outerIterator.returned !== 0)
        throw new Error("bad value: " + outerIterator.returned);
}());

(function test() {
    var outerIterator = createIterator();
    var innerIterator = createIterator();
    outer: for (var e1 of outerIterator) {
        inner: for (var e2 of innerIterator) {
            break;
        }
    }
    if (innerIterator.returned !== 6)
        throw new Error("bad value: " + innerIterator.returned);
    if (outerIterator.returned !== 0)
        throw new Error("bad value: " + outerIterator.returned);
}());

(function test() {
    var outerIterator = createIterator();
    var innerIterator = createIterator();
    outer: for (var e1 of outerIterator) {
        inner: for (var e2 of innerIterator) {
            break;
        }
    }
    if (innerIterator.returned !== 6)
        throw new Error("bad value: " + innerIterator.returned);
    if (outerIterator.returned !== 0)
        throw new Error("bad value: " + outerIterator.returned);
}());

(function test() {
    var outerIterator = createIterator();
    var innerIterator = createIterator();
    outer: for (var e1 of outerIterator) {
        inner: for (var e2 of innerIterator) {
            continue;
        }
    }
    if (innerIterator.returned !== 0)
        throw new Error("bad value: " + innerIterator.returned);
    if (outerIterator.returned !== 0)
        throw new Error("bad value: " + outerIterator.returned);
}());

(function test() {
    var outerIterator = createIterator();
    var innerIterator = createIterator();
    outer: for (var e1 of outerIterator) {
        inner: for (var e2 of innerIterator) {
            continue inner;
        }
    }
    if (innerIterator.returned !== 0)
        throw new Error("bad value: " + innerIterator.returned);
    if (outerIterator.returned !== 0)
        throw new Error("bad value: " + outerIterator.returned);
}());

(function test() {
    var outerIterator = createIterator();
    var innerIterator = createIterator();
    outer: for (var e1 of outerIterator) {
        inner: for (var e2 of innerIterator) {
            continue outer;
        }
    }
    if (innerIterator.returned !== 6)
        throw new Error("bad value: " + innerIterator.returned);
    if (outerIterator.returned !== 0)
        throw new Error("bad value: " + outerIterator.returned);
}());

(function test() {
    var outerIterator = createIterator();
    var innerIterator = createIterator();
    (function () {
        outer: for (var e1 of outerIterator) {
            inner: for (var e2 of innerIterator) {
                return;
            }
        }
    }());
    if (innerIterator.returned !== 1)
        throw new Error("bad value: " + innerIterator.returned);
    if (outerIterator.returned !== 1)
        throw new Error("bad value: " + outerIterator.returned);
}());

(function test() {
    var outerIterator = createIterator();
    var innerIterator = createIterator();
    (function () {
        outer: for (var e1 of outerIterator) {
            inner: for (var e2 of innerIterator) {
            }
            return;
        }
    }());
    if (innerIterator.returned !== 0)
        throw new Error("bad value: " + innerIterator.returned);
    if (outerIterator.returned !== 1)
        throw new Error("bad value: " + outerIterator.returned);
}());


(function test() {
    function raiseError() {
        throw new Error("Cocoa");
    }
    var outerIterator = createIterator();
    var innerIterator = createIterator();
    (function () {
        var error = null;
        try {
            outer: for (var e1 of outerIterator) {
                inner: for (var e2 of innerIterator) {
                    raiseError();
                }
            }
        } catch (e) {
            error = e;
        }
        if (innerIterator.returned !== 1)
            throw new Error("bad value: " + innerIterator.returned);
        if (outerIterator.returned !== 1)
            throw new Error("bad value: " + outerIterator.returned);
        if (!error)
            throw new Error("not thrown");
        if (String(error) !== "Error: Cocoa")
            throw new Error("bad error: " + String(error));
    }());
}());
