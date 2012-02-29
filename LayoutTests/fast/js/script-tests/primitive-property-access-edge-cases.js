description(
"This page tests for assertion failures in edge cases of property lookup on primitive values."
);

var didNotCrash = true;

(function () {
    delete String.prototype.constructor;
    for (var i = 0; i < 3; ++i)
        "".replace;
})();

(function () {
    String.prototype.__proto__ = { x: 1, y: 1 };
    delete String.prototype.__proto__.x;
    for (var i = 0; i < 3; ++i)
        "".y;
})();

(function () {
    function f(x) {
        x.y;
    }
    
    String.prototype.x = 1;
    String.prototype.y = 1;
    delete String.prototype.x;

    Number.prototype.x = 1;
    Number.prototype.y = 1;
    delete Number.prototype.x;

    for (var i = 0; i < 3; ++i)
        f("");

    for (var i = 0; i < 3; ++i)
        f(.5);
})();


var checkOkay;

function checkGet(x, constructor)
{
    checkOkay = false;
    Object.defineProperty(constructor.prototype, "foo", { get: function() { checkOkay = typeof this === 'object'; }, configurable: true });
    x.foo;
    delete constructor.prototype.foo;
    return checkOkay;
}

function checkSet(x, constructor)
{
    checkOkay = false;
    Object.defineProperty(constructor.prototype, "foo", { set: function() { checkOkay = typeof this === 'object'; }, configurable: true });
    x.foo = null;
    delete constructor.prototype.foo;
    return checkOkay;
}

function checkGetStrict(x, constructor)
{
    checkOkay = false;
    Object.defineProperty(constructor.prototype, "foo", { get: function() { "use strict"; checkOkay = typeof this !== 'object'; }, configurable: true });
    x.foo;
    delete constructor.prototype.foo;
    return checkOkay;
}

function checkSetStrict(x, constructor)
{
    checkOkay = false;
    Object.defineProperty(constructor.prototype, "foo", { set: function() { "use strict"; checkOkay = typeof this !== 'object'; }, configurable: true });
    x.foo = null;
    delete constructor.prototype.foo;
    return checkOkay;
}

shouldBeTrue("checkGet(1, Number)");
shouldBeTrue("checkGet('hello', String)");
shouldBeTrue("checkGet(true, Boolean)");
shouldBeTrue("checkSet(1, Number)");
shouldBeTrue("checkSet('hello', String)");
shouldBeTrue("checkSet(true, Boolean)");
shouldBeTrue("checkGetStrict(1, Number)");
shouldBeTrue("checkGetStrict('hello', String)");
shouldBeTrue("checkGetStrict(true, Boolean)");
shouldBeTrue("checkSetStrict(1, Number)");
shouldBeTrue("checkSetStrict('hello', String)");
shouldBeTrue("checkSetStrict(true, Boolean)");

function checkRead(x, constructor)
{
    return x.foo === undefined;
}

function checkWrite(x, constructor)
{
    x.foo = null;
    return x.foo === undefined;
}

function checkReadStrict(x, constructor)
{
    "use strict";
    return x.foo === undefined;
}

function checkWriteStrict(x, constructor)
{
    "use strict";
    x.foo = null;
    return x.foo === undefined;
}

shouldBeTrue("checkRead(1, Number)");
shouldBeTrue("checkRead('hello', String)");
shouldBeTrue("checkRead(true, Boolean)");
shouldBeTrue("checkWrite(1, Number)");
shouldBeTrue("checkWrite('hello', String)");
shouldBeTrue("checkWrite(true, Boolean)");
shouldBeTrue("checkReadStrict(1, Number)");
shouldBeTrue("checkReadStrict('hello', String)");
shouldBeTrue("checkReadStrict(true, Boolean)");
shouldThrow("checkWriteStrict(1, Number)");
shouldThrow("checkWriteStrict('hello', String)");
shouldThrow("checkWriteStrict(true, Boolean)");

shouldBeTrue("didNotCrash");
