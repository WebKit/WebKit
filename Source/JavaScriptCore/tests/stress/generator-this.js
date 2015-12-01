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


var global = new Function('return this')();

(function () {
    function *gen() {
        yield this;
    }

    {
        let g = gen();
        shouldBe(g.next().value, global);
    }
    {
        let g = new gen();
        shouldThrow(() => {
            g.next();
        }, `ReferenceError: Cannot access uninitialized variable.`);
    }
    {
        let thisObject = {};
        let g = gen.call(thisObject);
        shouldBe(g.next().value, thisObject);
    }
}());

(function () {
    function *gen() {
        "use strict";
        yield this;
    }

    {
        let g = gen();
        shouldBe(g.next().value, undefined);
    }
    {
        let g = new gen();
        shouldThrow(() => {
            g.next();
        }, `ReferenceError: Cannot access uninitialized variable.`);
    }
    {
        let thisObject = {};
        let g = gen.call(thisObject);
        shouldBe(g.next().value, thisObject);
    }
}());
