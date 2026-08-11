function truth() {
    return true;
}
noInline(truth);

function assert(cond) {
    if (!cond)
        throw new Error("broke assertion");
}
noInline(assert);

;(function() {

function foo(i) {
    delete x;
    let x = i;
    assert(x === i);
}
function bar(i) {
    delete x;
    let x = i;
    function capX() { return x; }
    assert(capX() === i);
}

for (var i = 0; i < 1000; i++) {
    foo(i);
    bar(i);
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

    var hadSyntaxErrorForEval = false;
    try {
        if (truth()) {
            let x = 40;
            eval("var x = 20;");
        }
    } catch (e) {
        hadSyntaxErrorForEval = e instanceof SyntaxError;
    }

    assert(hadSyntaxErrorForEval);
    assert(typeof x === "undefined");
}

function bar() {
    var hadError = false;
    try {
        x;
    } catch(e) {
        hadError = true;
    }
    assert(hadError);

    var hadSyntaxErrorForEval = false;
    try {
        if (truth()) {
            let x = 40;
            function capX() { return x; }
            eval("var x = 20;");
        } 
    } catch (e) {
        hadSyntaxErrorForEval = e instanceof SyntaxError;
    }

    assert(hadSyntaxErrorForEval);
    assert(typeof x === "undefined");
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
