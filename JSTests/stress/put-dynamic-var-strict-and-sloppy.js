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

(function () {
    var flag = true;
    var scope = {
        resolveStrict: 20,
        resolveSloppy: 20,
    };

    with (scope) {
        var putValueStrict = function (text, value)
        {
            if (flag)
                eval(text); // Make resolution Dynamic.
            var result = (function () {
                "use strict";
                resolveStrict = value;
            }());
            return result;
        };
        noInline(putValueStrict);

        var resolveSloppy = 20;
        var putValueSloppy = function (text, value)
        {
            if (flag)
                eval(text); // Make resolution Dynamic.
            var result = (function () {
                resolveSloppy = value;
            }());
            return result;
        }
        noInline(putValueSloppy);
    }

    putValueStrict(`var resolveStrict = 20`, i);
    putValueSloppy(`var resolveSloppy = 20`, i);
    flag = false;

    for (var i = 0; i < 4e3; ++i) {
        putValueStrict(``, i);
        shouldBe(scope.resolveStrict, i);
        putValueSloppy(``, i);
        shouldBe(scope.resolveSloppy, i);
    }
    Object.freeze(scope);
    shouldThrow(() => {
        putValueStrict(``, 0);
    }, `TypeError: Attempted to assign to readonly property.`);
    putValueSloppy(``, 0);
    shouldBe(scope.resolveSloppy, 4e3 - 1);
}());
