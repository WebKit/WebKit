function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}, expected: ${expected}`);
}

function shouldBeSyntaxError(source) {
    let error;
    try {
        new Function(source);
    } catch (e) {
        error = e;
    }
    if (!(error instanceof SyntaxError))
        throw new Error(`Expected a SyntaxError: ${source}`);
}

shouldBe(global1(), 1);
label: function global1() { return 1; }
shouldBe(Object.getOwnPropertyDescriptor(globalThis, "global1").configurable, false);

shouldBe(typeof global2, "undefined");
{
    shouldBe(global2(), 2);
    label: function global2() { return 2; }
}
shouldBe(global2(), 2);

(function () {
    shouldBe(f(), 1);
    label: function f() { return 1; }
    shouldBe(f(), 1);
})();

(function () {
    shouldBe(f(), 1);
    label1: label2: label3: function f() { return 1; }
})();

(function () {
    shouldBe(f(), 2);
    function f() { return 1; }
    label: function f() { return 2; }
    shouldBe(f(), 2);
})();

(function () {
    shouldBe(f(), 1);
    label: function f() { return 2; }
    function f() { return 1; }
    shouldBe(f(), 1);
})();

(function () {
    var f = 1;
    label: function f() { }
    shouldBe(f, 1);
})();

(function (f) {
    shouldBe(typeof f, "function");
    label: function f() { }
})(1);

(function (f) {
    {
        shouldBe(typeof f, "function");
        label: function f() { }
    }
    shouldBe(f, 1);
})(1);

(function () {
    shouldBe(typeof arguments, "function");
    label: function arguments() { }
})();

(function () {
    shouldBe(f, undefined);
    {
        shouldBe(f(), 1);
        label: function f() { return 1; }
        shouldBe(f(), 1);
    }
    shouldBe(f(), 1);
})();

(function () {
    {
        shouldBe(f(), 1);
        label1: label2: function f() { return 1; }
    }
    shouldBe(f(), 1);
})();

(function () {
    {
        shouldBe(f(), 2);
        function f() { return 1; }
        shouldBe(f(), 2);
        label: function f() { return 2; }
        shouldBe(f(), 2);
    }
    shouldBe(f(), 2);
})();

(function () {
    {
        shouldBe(f(), 2);
        label1: function f() { return 1; }
        label2: function f() { return 2; }
    }
    shouldBe(f(), 2);
})();

(function () {
    let f = 1;
    {
        shouldBe(f(), 2);
        label: function f() { return 2; }
    }
    shouldBe(f, 1);
})();

(function () {
    {
        label: function f() { return 1; }
        f = 2;
        shouldBe(f, 2);
    }
    shouldBe(f(), 1);
})();

(function () {
    for (let i = 0; i < 2; i++) {
        shouldBe(f(), i);
        label: function f() { return i; }
    }
})();

(function () {
    switch (1) {
    case 1:
        shouldBe(f(), 1);
    case 2:
        label: function f() { return 1; }
    }
    shouldBe(f(), 1);
})();

(function () {
    switch (1) {
    case 0:
        label: function f() { return 1; }
    case 1:
        shouldBe(f(), 1);
    }
    shouldBe(f, undefined);
})();

(function () {
    try {
        throw 1;
    } catch (e) {
        shouldBe(f(), 1);
        label: function f() { return 1; }
    }
    shouldBe(f(), 1);
})();

(function* () {
    shouldBe(f(), 1);
    label: function f() { return 1; }
})().next();

(() => {
    shouldBe(f(), 1);
    label: function f() { return 1; }
})();

(function () {
    shouldBe(eval("f(); label: function f() { return 1; }"), 1);
    shouldBe(f(), 1);
    shouldBe(eval("2; label: function g() { }"), 2);
})();

(function () {
    let f = 1;
    let error;
    try {
        eval("label: function f() { }");
    } catch (e) {
        error = e;
    }
    shouldBe(error instanceof SyntaxError, true);
    eval("{ label: function f() { } }");
    shouldBe(f, 1);
})();

shouldBeSyntaxError("let f; label: function f() { }");
shouldBeSyntaxError("label: function f() { } let f;");
shouldBeSyntaxError("const f = 1; label: function f() { }");
shouldBeSyntaxError("class f { } label: function f() { }");
shouldBeSyntaxError("{ let f; label: function f() { } }");
shouldBeSyntaxError("{ label: function f() { } let f; }");
shouldBeSyntaxError("{ label: function f() { } class f { } }");
shouldBeSyntaxError("{ var f; label: function f() { } }");
shouldBeSyntaxError("{ label: function f() { } var f; }");
shouldBeSyntaxError("{ label: function f() { } { var f; } }");
shouldBeSyntaxError("{ label: function f() { } function* f() { } }");
shouldBeSyntaxError("{ async function f() { } label: function f() { } }");
shouldBeSyntaxError("switch (1) { case 0: let f; case 1: label: function f() { } }");
shouldBeSyntaxError("try { } catch (f) { label: function f() { } }");
shouldBeSyntaxError("try { } catch ({ f }) { label: function f() { } }");

shouldBeSyntaxError("'use strict'; label: function f() { }");
shouldBeSyntaxError("'use strict'; { label: function f() { } }");
shouldBeSyntaxError("label: function* f() { }");
shouldBeSyntaxError("{ label: function* f() { } }");
shouldBeSyntaxError("if (1) label: function f() { }");
shouldBeSyntaxError("if (1) ; else label: function f() { }");
shouldBeSyntaxError("while (0) label: function f() { }");
shouldBeSyntaxError("do label: function f() { } while (0)");
shouldBeSyntaxError("for (;;) label: function f() { }");
shouldBeSyntaxError("for (var x in { }) label: function f() { }");
shouldBeSyntaxError("for (var x of []) label: function f() { }");
shouldBeSyntaxError("with ({ }) label: function f() { }");
shouldBeSyntaxError("label1: if (1) label2: function f() { }");
