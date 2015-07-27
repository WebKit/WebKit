function truth() {
    return true;
}
noInline(truth);

function assert(cond) {
    if (!cond)
        throw new Error("broke assertion");
}
noInline(assert);

function shouldThrowTDZ(func) {
    var hasThrown = false;
    try {
        func();
    } catch(e) {
        if (e.name.indexOf("ReferenceError") !== -1)
            hasThrown = true;
    }
    assert(hasThrown);
}
noInline(shouldThrowTDZ);

;(function() {

function foo() {
    delete x;
    let x;
}
function bar() {
    delete x;
    let x;
    function capX() { return x; }
}

for (var i = 0; i < 1000; i++) {
    shouldThrowTDZ(foo);
    shouldThrowTDZ(bar);
}

})();


;(function() {

function foo() {
    var hadError = false;
    try {
        x;
    } catch(e) {
        hadError = true;
    }
    assert(hadError);

    if (truth()) {
        // This eval is enterpreted as follows:
        // eval("var x; x = 20");
        // We first assign undefined to the "var x".
        // Then, we interperet an assignment expression
        // into the resolved variable x. x resolves to the lexical "let x;"
        // Look at ECMA section 13.3.2.4 of the ES6 spec:
        // http://www.ecma-international.org/ecma-262/6.0/index.html#sec-variable-statement-runtime-semantics-evaluation
        // And also look at section 8.3.1 ResolveBinding:
        // http://www.ecma-international.org/ecma-262/6.0/index.html#sec-resolvebinding
        let x = 40;
        eval("var x = 20;");
        assert(x === 20);
    }
    assert(x === undefined);
}

function bar() {
    var hadError = false;
    try {
        x;
    } catch(e) {
        hadError = true;
    }
    assert(hadError);

    if (truth()) {
        let x = 40;
        function capX() { return x; }
        eval("var x = 20;");
        assert(x === 20);
    }
    assert(x === undefined);
}

function baz() {
    if (truth()) {
        let x = 40;
        eval("let x = 20; assert(x === 20);");
        assert(x === 40);
    }
    if (truth()) {
        let x = 40;
        function capX() { return x; }
        eval("let x = 20; assert(x === 20);");
        assert(x === 40);
    }
}

function baz() {
    // Test eval() caching.
    let x = 20;
    let evalString = "x;";

    assert(eval(evalString) === 20);
    if (truth()) {
        let y = 60;
        assert(eval(evalString) === 20);
        assert(y === 60);
        if (truth()) {
            let y = 50, z = 70, x = 40;
            assert(eval(evalString) === 40);
            assert(y === 50 && z === 70);
        }
    }
}

for (var i = 0; i < 100; i++) {
    foo();
    bar();
    baz();
}

})();
