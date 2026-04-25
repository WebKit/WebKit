function assert(cond, msg = "") {
    if (!cond)
        throw new Error(msg);
}
noInline(assert);

function shouldThrow(func) {
    var hadError = false;
    try {
        func()
    } catch (e) {
        hadError = true;
    }
    assert(hadError, "Did not throw");
}
noInline(shouldThrow);

function shouldThrowSyntaxError(str) {
    var hadError = false;
    try {
        eval(str);
    } catch (e) {
        if (e instanceof SyntaxError)
            hadError = true;
    }
    assert(hadError, "Did not throw syntax error");
}
noInline(shouldThrowSyntaxError);

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

function basic(x, y=x) {
    assert(y === x, "basics don't work.")
}
basic(20);
basic("hello");
basic({foo: 20});
basic(undefined);

;(function(){
    var scopeVariable = {hello: "world"};
    function basicScope(x = scopeVariable) {
        assert(x === scopeVariable);
    }
    basicScope();
})();

function basicFunctionCaptureInDefault(theArg = 20, y = function() {return theArg}) {
    assert(theArg === y(), "y should return x.");
    theArg = {};
    assert(theArg === y(), "y should return x.");
}
basicFunctionCaptureInDefault()
basicFunctionCaptureInDefault(undefined)

function basicCaptured(x = 20, y = x) {
    assert(x === y, "y should equal x");
    function mutate() { x = "mutation"; }
    mutate()
    assert(x !== y, "y should not equal x");
}
basicCaptured()
basicCaptured(undefined)

function tricky(globalX = (globalX = "x"), y = (globalX = 20)) {
    assert(globalX === 20);
    assert(globalX === y);
}
shouldThrow(tricky);

function strict(x, y = x) {
    assert(x === y);
}
strict(20);
strict(undefined);

function playground(x = "foo", y = "bar") {
    return {x, y}
}
assert(playground().x === "foo")
assert(playground(undefined).x === "foo")
assert(playground(undefined, 20).x === "foo")
assert(playground(null).x === null)
assert(playground().y === "bar")
assert(playground(undefined, undefined).y === "bar")
assert(playground("hello", undefined).y === "bar")
assert(playground("bar").x === playground(undefined, undefined).y)
assert(playground(10).x === 10)
assert(playground(undefined, 20).y === 20)
assert(playground(undefined, null).y === null)

function scoping(f = function() { return local;}) {
    shouldThrow(f);
    var local = 10;
    shouldThrow(f);
}
scoping();

function augmentsArguments1(x = 20) {
    assert(x === 20);

    arguments[0] = 10;
    assert(x === 20);

    x = 15;
    assert(x === 15);
    assert(arguments[0] === 10);
}
augmentsArguments1(undefined);

function augmentsArguments2(x = 20) {
    assert(x === 20);

    arguments[0] = 10;
    assert(x === 20);
    assert(arguments[0] === 10);

    x = 15;
    assert(x === 15);
    assert(arguments[0] === 10);

    function augment() { x = 40 }
    augment()
    assert(x === 40);
    assert(arguments[0] === 10);
}
augmentsArguments2(undefined);

function augmentsArguments3(x = 10) {
    assert(x === 10);

    assert(arguments[0] === undefined);
    x = 20;
    assert(arguments[0] === undefined);
}
augmentsArguments3();

function augmentsArguments4(x) {
    "use strict";
    function inner(x = 10) {
        assert(x === 10);

        assert(arguments[0] === undefined);
        x = 20;
        assert(arguments[0] === undefined);
    }
    inner(x);
}
augmentsArguments4();
augmentsArguments4(undefined);

function augmentsArguments5(x) {
    "use strict";
    function inner(x = 10) {
        assert(x === 20);

        assert(arguments[0] === 20);
        x = 20;
        assert(arguments[0] === 20);
    }
    inner(x);
}
augmentsArguments5(20);

;(function () {
    var outer = "outer";
    function foo(a = outer, b = function() { return a; }, c = function(v) { a = v; }) {
        var a;
        assert(a === "outer");
        a = 20;
        assert(a === 20);
        assert(b() === "outer");
        c("hello");
        assert(b() === "hello");
    }

    function bar(a = outer, b = function() { return a; }, c = function(v) { a = v; }) {
        with({}) {
            var a;
            assert(a === "outer");
            a = 20;
            assert(a === 20);
            assert(b() === "outer");
            c("hello");
            assert(b() === "hello");
        }
    }

    function baz(x = function() { return y; }, y = "y") {
        assert(x() === "y");
        assert(x() === y);
        assert(y === y);
    }

    function jaz(x = function() { return y; }, y = "y") {
        return x;
    }

    function taz(x = 10, y = eval("x + 1")) {
        assert(y === 11);
    }

    for (var i = 0; i < 1000; i++) {
        foo();
        bar();
        baz();
        assert(jaz(undefined, 20)() === 20);
        assert(jaz(undefined, undefined)() === "y");
        assert(jaz(undefined, {x: "x"})().x === "x");
        taz();
    }
})();

// Length

function named1(a){};
function named2(a=1){};
function named3(a,b){};
function named4(a,b=1){};
function named5(a=1,b=1){};
function named6(a,b,c){};
function named7(a=1,b,c){};
function named8(a,b=1,c){};
function named9(a,b,c=1){};
function named10(a,...b){};
function named11(a=1,...b){};
function named12(a=10,b,c=20,...r){};

assert( named1.length === 1 );
assert( named2.length === 0 );
assert( named3.length === 2 );
assert( named4.length === 1 );
assert( named5.length === 0 );
assert( named6.length === 3 );
assert( named7.length === 0 );
assert( named8.length === 1 );
assert( named9.length === 2 );
assert( named10.length === 1 );
assert( named11.length === 0 );
assert( named12.length === 0 );

assert( (function(a){}).length === 1 );
assert( (function(a=1){}).length === 0 );
assert( (function(a,b){}).length === 2 );
assert( (function(a,b=1){}).length === 1 );
assert( (function(a=1,b=1){}).length === 0 );
assert( (function(a,b,c){}).length === 3 );
assert( (function(a=1,b,c){}).length === 0 );
assert( (function(a,b=1,c){}).length === 1 );
assert( (function(a,b,c=1){}).length === 2 );
assert( (function(a,...b){}).length === 1 );
assert( (function(a=1,...b){}).length === 0 );
assert( (function(a=10,b,c=20,...r){}).length === 0 );

assert( (function*(a){}).length === 1 );
assert( (function*(a=1){}).length === 0 );
assert( (function*(a,b){}).length === 2 );
assert( (function*(a,b=1){}).length === 1 );
assert( (function*(a=1,b=1){}).length === 0 );
assert( (function*(a,b,c){}).length === 3 );
assert( (function*(a=1,b,c){}).length === 0 );
assert( (function*(a,b=1,c){}).length === 1 );
assert( (function*(a,b,c=1){}).length === 2 );
assert( (function*(a,...b){}).length === 1 );
assert( (function*(a=1,...b){}).length === 0 );
assert( (function*(a=10,b,c=20,...r){}).length === 0 );

assert( ((a)=>{}).length === 1 );
assert( ((a=1)=>{}).length === 0 );
assert( ((a,b)=>{}).length === 2 );
assert( ((a,b=1)=>{}).length === 1 );
assert( ((a=1,b=1)=>{}).length === 0 );
assert( ((a,b,c)=>{}).length === 3 );
assert( ((a=1,b,c)=>{}).length === 0 );
assert( ((a,b=1,c)=>{}).length === 1 );
assert( ((a,b,c=1)=>{}).length === 2 );
assert( ((a,...b)=>{}).length === 1 );
assert( ((a=1,...b)=>{}).length === 0 );
assert( ((a=10,b,c=20,...r)=>{}).length === 0 );

assert( Object.getOwnPropertyDescriptor({set setter(a){}}, "setter").set.length === 1 );
assert( Object.getOwnPropertyDescriptor({set setter(a=1){}}, "setter").set.length === 0 );

assert( ({method(a){}}).method.length === 1 );
assert( ({method(a=1){}}).method.length === 0 );
assert( ({method(a,b){}}).method.length === 2 );
assert( ({method(a,b=1){}}).method.length === 1 );
assert( ({method(a=1,b=1){}}).method.length === 0 );
assert( ({method(a,b,c){}}).method.length === 3 );
assert( ({method(a=1,b,c){}}).method.length === 0 );
assert( ({method(a,b=1,c){}}).method.length === 1 );
assert( ({method(a,b,c=1){}}).method.length === 2 );
assert( ({method(a,...b){}}).method.length === 1 );
assert( ({method(a=1,...b){}}).method.length === 0 );
assert( ({method(a=10,b,c=20,...r){}}).method.length === 0 );

assert( ({*method(a){}}).method.length === 1 );
assert( ({*method(a=1){}}).method.length === 0 );
assert( ({*method(a,b){}}).method.length === 2 );
assert( ({*method(a,b=1){}}).method.length === 1 );
assert( ({*method(a=1,b=1){}}).method.length === 0 );
assert( ({*method(a,b,c){}}).method.length === 3 );
assert( ({*method(a=1,b,c){}}).method.length === 0 );
assert( ({*method(a,b=1,c){}}).method.length === 1 );
assert( ({*method(a,b,c=1){}}).method.length === 2 );
assert( ({*method(a,...b){}}).method.length === 1 );
assert( ({*method(a=1,...b){}}).method.length === 0 );
assert( ({*method(a=10,b,c=20,...r){}}).method.length === 0 );

assert( (class {static method(a){}}).method.length === 1 );
assert( (class {static method(a=1){}}).method.length === 0 );
assert( (class {static method(a,b){}}).method.length === 2 );
assert( (class {static method(a,b=1){}}).method.length === 1 );
assert( (class {static method(a=1,b=1){}}).method.length === 0 );
assert( (class {static method(a,b,c){}}).method.length === 3 );
assert( (class {static method(a=1,b,c){}}).method.length === 0 );
assert( (class {static method(a,b=1,c){}}).method.length === 1 );
assert( (class {static method(a,b,c=1){}}).method.length === 2 );
assert( (class {static method(a,...b){}}).method.length === 1 );
assert( (class {static method(a=1,...b){}}).method.length === 0 );
assert( (class {static method(a=10,b,c=20,...r){}}).method.length === 0 );

// TDZ errors.
;(function() {
    function basicError(x = y, y) { }
    function basicError2(x = x) { }
    function baz(z = {p: x}, x = z) {}
    function bar(x = {p: [x]}) {}
    function jaz(x = eval("y"), y) { }
    function kaz(x = eval(";(function() { return y})();"), y) { }
    for (var i = 0; i < 1000; i++) {
        shouldThrowTDZ(basicError);
        shouldThrowTDZ(basicError2);
        shouldThrowTDZ(baz);
        shouldThrowTDZ(bar);
        shouldThrowTDZ(jaz);
        shouldThrowTDZ(kaz);
    }
})();


// Test proper variable binding.
;(function() {
    function foo(a = function() { return b; }, {b}) {
        assert(a() === 34);
        assert(b === 34);
        b = 50;
        assert(a() === 50);
        assert(b === 50);
    }
    function bar(a = function(x) { b = x; }, {b}) {
        assert(b === 34);
        a(50);
        assert(b === 50);
    }
    function baz(f1 = function(x) { b = x; }, f2 = function() { return b; }, {b}) {
        var b;
        assert(b === 34);
        assert(f2() === 34);
        f1(50);
        assert(b === 34);
        assert(f2() === 50);
    }
    noInline(foo);
    noInline(bar);
    noInline(baz);
    for (let i = 0; i < 1000; i++) {
        foo(undefined, {b: 34});
        bar(undefined, {b: 34});
        baz(undefined, undefined, {b: 34});
    }
})();


// Syntax errors.
shouldThrowSyntaxError("function b(a = 20, a = 40) {}");
shouldThrowSyntaxError("function b(aaaaa = 20,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v, aaaaa = 40) {}");
shouldThrowSyntaxError("function b(a = 20, {a}) {}");
shouldThrowSyntaxError("function b(a = 20, a) {}");
shouldThrowSyntaxError("function b(a, a = 20) {}");
shouldThrowSyntaxError("function b(a, a, b = 20) {}");
shouldThrowSyntaxError("function b({a, a} = 20) {}");
shouldThrowSyntaxError("function b({a, a} = 20) {}");
