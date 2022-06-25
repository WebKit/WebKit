function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
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
        throw new Error("not thrown");
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

(function () {
    function * generator() {
        yield * (function * () {
            try {
                yield "foo";
            } catch(e) {
                return;
            }
        }());
        // OK, continue executing.
        yield "bar";
    }
    var iter = generator();
    iter.next();
    shouldBe(iter["throw"]().value, "bar");
}());

(function () {
    function * generator() {
        yield * (function * () {
            try {
                yield "foo";
            } catch (e) {
                throw e;
            }
        }());
        // OK, continue executing.
        yield "bar";
    }
    var iter = generator();
    iter.next();
    shouldThrow(() => {
        iter["throw"](new Error("NG"));
    }, `Error: NG`);
}());

(function () {
    function * generator() {
        yield * (function * () {
            try {
                yield "foo";
            } catch (e) {
            }
            yield "cocoa";
        }());
        // OK, continue executing.
        yield "bar";
    }
    var iter = generator();
    iter.next();
    shouldBe(iter["throw"]().value, "cocoa");
    shouldBe(iter.next().value, "bar");
}());
