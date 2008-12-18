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

shouldBeTrue("didNotCrash");

var successfullyParsed = true;
