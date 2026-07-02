var assert = function (result, expected) {
    if (result !== expected) {
        throw new Error('Error in assert. Expected-' + expected + ' but was ' + result);
    }
};

var assertThrow = function (cb, expected) {
    let error = null;
    try {
        cb();
    } catch(e) {
        error = e;  
    }
    if (error === null) {
        throw new Error('Error is expected. Expected "' + expected + '" but error was not thrown."');
    }
    if (error.toString() !== expected) {
        throw new Error('Error is expected. Expected "' + expected + '" but error was "' + error + '"');
    }
};

var assertNotThrow = function (cb) {
    try  {
        cb();   
    } catch (e) {
       throw new Error('Error is not expected. But was throw error: "' + e + '"');
    }
}

var result = 0;

eval("'use strict'; let y = 5; function f() { result = y;} f()");
assert(result, 5);

eval("'use strict'; const x = 10; function f() { result = x;} f()");
assert(result, 10);

eval("'use strict'; class A { constructor() { this.id = 'foo'; } }; function foo() { return (new A).id; }; result = foo();");
assert(result, 'foo');


eval("let y = 15; function f() { result = y;} f()");
assert(result, 15);

eval("const x = 20; function f() { result = x;} f()");
assert(result, 20);

eval("class B { constructor() { this.id = 'boo'; } }; function foo() { return (new B).id; }; result = foo();");
assert(result, 'boo');

function foo () {
    var res = 0;

    eval("'use strict'; let y = 5; function f() { res = y;} f()");
    assert(res, 5);

    eval("'use strict'; const x = 10; function f() { result = x;} f()");
    assert(res, 10);

    eval("'use strict'; class C { constructor() { this.id = 'foo'; } }; function foo() { return (new C).id; }; result = foo();");
    assert(res, 'foo');


    eval("let y = 15; function f() { result = y;} f()");
    assert(res, 15);

    eval("const x = 20; function f() { result = x;} f()");
    assert(res, 20);

    eval("class D { constructor() { this.id = 'boo'; } }; function foo() { return (new D).id; }; result = foo();");
    assert(res, 'boo');
}

foo();

function boo () {
    {
        let f;
        eval("'use strict'; let y = 5; function f() { result = y;} f()");
        assert(typeof f, 'undefined');

        eval("'use strict'; const x = 10; function f() { result = x;} f()");
        assert(typeof f, 'undefined');

        eval("'use strict'; class E { constructor() { this.id = 'foo'; } }; function f() { return (new E).id; }; result = f();");
        assert(typeof f, 'undefined');
    }
}

function bar () {
    {
        let f;
        eval("let y = 15; function f() { result = y;} f()");
    }
}

function foobar () {
    {
        let f;
        eval("const x = 20; function f() { result = x; } f()");
    }
}

function baz() {
    {
        let f;
        eval("var x = 20; function f() { result = x; } f()");
    }   
}

function barWithTry () {
    var error; 
    {
        let f;
        try {
            eval("let y = 15; function f() { result = y;} f()");
        } catch (err) {
            error = err;
        }
        assert(typeof f, 'undefined');
    }
    assert(error.toString(), 'SyntaxError: Can\'t create duplicate variable in eval: \'f\'');
}

function foobarWithTry () {
    var error; 
    {
        let f;
        try {
            eval("const x = 20; function f() { result = x; } f()");
        } catch (err) {
            error = err;
        }
        assert(typeof f, 'undefined');
    }
    assert(error.toString(), 'SyntaxError: Can\'t create duplicate variable in eval: \'f\'');
}

function bazWithTry () {
    var error; 
    {
        let f;
        try {
            eval("var x = 20; function f() { result = x; } f()");
        } catch (err) {
            error = err;
        }
        assert(typeof f, 'undefined');
    }
    assert(error.toString(), 'SyntaxError: Can\'t create duplicate variable in eval: \'f\'');
}

assertNotThrow(() => boo());
assertThrow(() => bar(), 'SyntaxError: Can\'t create duplicate variable in eval: \'f\'');
assertThrow(() => foobar(), 'SyntaxError: Can\'t create duplicate variable in eval: \'f\'');
assertThrow(() => baz(), 'SyntaxError: Can\'t create duplicate variable in eval: \'f\'');

barWithTry();
foobarWithTry();
bazWithTry();
