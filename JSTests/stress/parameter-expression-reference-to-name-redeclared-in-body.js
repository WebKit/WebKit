// A parameter expression cannot see the declarations in its function's body, so a name that
// it references and that the body redeclares must still be captured from an enclosing scope.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + ", expected: " + expected);
}

function throughIIFE() {
    var x = 1;
    return (function () {
        function g(a = x) { var x; return a; }
        return g();
    })();
}

function throughDeclaration() {
    var x = 1;
    function inner() {
        function g(a = x) { var x; return a; }
        return g();
    }
    return inner();
}

function throughNestedIIFEs() {
    var x = 1;
    return (function () {
        return (function () {
            return (function () {
                function g(a = x) { var x; return a; }
                return g();
            })();
        })();
    })();
}

function invokedImmediately() {
    var x = 1;
    return (function () {
        return (function g(a = x) { var x; return a; })();
    })();
}

function bodyDeclaresLet() {
    var x = 1;
    return (function () {
        function g(a = x) { let x; return a; }
        return g();
    })();
}

function bodyDeclaresFunction() {
    var x = 1;
    return (function () {
        function g(a = x) { function x() { } return a; }
        return g();
    })();
}

function bodyDeclaresVarInBlock() {
    var x = 1;
    return (function () {
        function g(a = x) { { var x; } return a; }
        return g();
    })();
}

function bodyHoistsBlockFunction() {
    var x = 1;
    return (function () {
        function g(a = x) { { function x() { } } return a; }
        return g();
    })();
}

function bodyAlsoUsesName() {
    var x = 1;
    return (function () {
        function g(a = x) { var x = 2; return a + "," + x; }
        return g();
    })();
}

function closureInDefault() {
    var x = 1;
    return (function () {
        function g(a = () => x) { var x = 2; return a(); }
        return g();
    })();
}

function arrowFunction() {
    var x = 1;
    return (function () {
        return ((a = x) => { var x; return a; })();
    })();
}

function objectMethod() {
    var x = 1;
    return (function () {
        return ({ m(a = x) { var x; return a; } }).m();
    })();
}

function setter() {
    var x = 1;
    return (function () {
        var result;
        ({ set s(v = x) { var x; result = v; } }).s = undefined;
        return result;
    })();
}

function classMethod() {
    var x = 1;
    return (function () {
        return new (class { m(a = x) { var x; return a; } })().m();
    })();
}

function classConstructor() {
    var x = 1;
    return (function () {
        return new (class { constructor(a = x) { var x; this.a = a; } })().a;
    })();
}

function classBinding() {
    class C {
        m() {
            return (function () {
                function g(a = C) { var C; return a; }
                return g();
            })();
        }
    }
    return new C().m() === C;
}

function classField() {
    var x = 1;
    return (function () {
        return new (class { f = (function (a = x) { var x; return a; })(); })().f;
    })();
}

function classStaticBlock() {
    var x = 1;
    return (function () {
        var result;
        (class { static { result = (function (a = x) { var x; return a; })(); } });
        return result;
    })();
}

function strictMode() {
    "use strict";
    var x = 1;
    return (function () {
        function g(a = x) { var x; return a; }
        return g();
    })();
}

function generator() {
    var x = 1;
    return (function () {
        function* g(a = x) { var x; yield a; }
        return g().next().value;
    })();
}

function asyncFunction() {
    var x = 1;
    return (function () {
        var result;
        async function g(a = x) { var x; result = a; }
        g();
        return result;
    })();
}

function asyncArrowFunction() {
    var x = 1;
    return (function () {
        var result;
        (async (a = x) => { var x; result = a; })();
        return result;
    })();
}

function objectPatternDefault() {
    var x = 1;
    return (function () {
        function g({ a = x }) { var x; return a; }
        return g({ });
    })();
}

function arrayPatternDefault() {
    var x = 1;
    return (function () {
        function g([a = x]) { var x; return a; }
        return g([]);
    })();
}

function restPatternDefault() {
    var x = 1;
    return (function () {
        function g(...[a = x]) { var x; return a; }
        return g();
    })();
}

function computedPropertyKey() {
    var x = "k";
    return (function () {
        function g({ [x]: a }) { var x; return a; }
        return g({ k: 1 });
    })();
}

function blockScopedOuterBinding() {
    {
        let x = 1;
        return (function () {
            function g(a = x) { var x; return a; }
            return g();
        })();
    }
}

function catchParameter() {
    try {
        throw 1;
    } catch (e) {
        var g = function (a = e) { var e; return a; };
        return g();
    }
}

function catchParameterThroughIIFE() {
    try {
        throw 1;
    } catch (e) {
        return (function () {
            var g = function (a = e) { var e; return a; };
            return g();
        })();
    }
}

function arrowArgumentsInGenerator() {
    function* gen() {
        yield ((a = arguments) => { var arguments; return a; })();
    }
    return gen(1).next().value[0];
}

function ownArguments() {
    function g(a = arguments, b) { var arguments; return a[1]; }
    return g(undefined, 1);
}

function calleeName() {
    var f = 1;
    return (function () {
        return (function f(a = f) { var f; return typeof a; })();
    })();
}

function writeThroughDefault() {
    var writtenThroughDefault = 1;
    (function () {
        function g(a = (writtenThroughDefault = 7)) { var writtenThroughDefault; return a; }
        g();
    })();
    return writtenThroughDefault + "," + typeof globalThis.writtenThroughDefault;
}

function directEval() {
    return eval("let x = 1; (function () { function g(a = x) { var x; return a; } return g(); })()");
}

// A name that the parameter list declares refers to that parameter, not to an enclosing scope.
function laterParameterInTDZ() {
    var b = 1;
    return (function () {
        function g(a = b, b = 2) { var b; return a; }
        try {
            g();
        } catch (e) {
            return e instanceof ReferenceError;
        }
        return false;
    })();
}

shouldBe(throughIIFE(), 1);
shouldBe(throughDeclaration(), 1);
shouldBe(throughNestedIIFEs(), 1);
shouldBe(invokedImmediately(), 1);
shouldBe(bodyDeclaresLet(), 1);
shouldBe(bodyDeclaresFunction(), 1);
shouldBe(bodyDeclaresVarInBlock(), 1);
shouldBe(bodyHoistsBlockFunction(), 1);
shouldBe(bodyAlsoUsesName(), "1,2");
shouldBe(closureInDefault(), 1);
shouldBe(arrowFunction(), 1);
shouldBe(objectMethod(), 1);
shouldBe(setter(), 1);
shouldBe(classMethod(), 1);
shouldBe(classConstructor(), 1);
shouldBe(classBinding(), true);
shouldBe(classField(), 1);
shouldBe(classStaticBlock(), 1);
shouldBe(strictMode(), 1);
shouldBe(generator(), 1);
shouldBe(asyncFunction(), 1);
shouldBe(asyncArrowFunction(), 1);
shouldBe(objectPatternDefault(), 1);
shouldBe(arrayPatternDefault(), 1);
shouldBe(restPatternDefault(), 1);
shouldBe(computedPropertyKey(), 1);
shouldBe(blockScopedOuterBinding(), 1);
shouldBe(catchParameter(), 1);
shouldBe(catchParameterThroughIIFE(), 1);
shouldBe(arrowArgumentsInGenerator(), 1);
shouldBe(ownArguments(), 1);
shouldBe(calleeName(), "function");
shouldBe(writeThroughDefault(), "7,undefined");
shouldBe(directEval(), 1);
shouldBe(laterParameterInTDZ(), true);
