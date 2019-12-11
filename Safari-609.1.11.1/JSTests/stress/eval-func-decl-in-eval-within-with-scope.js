var assert = function (result, expected, message) {
    if (result !== expected) {
        throw new Error('Error in assert. Expected "' + expected + '" but was "' + result + '":' + message );
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

function foo(withScope, firstAssertValue,  secondAssertValue) {
    with (withScope) {
        eval("{ function f() { } }");
        assert(typeof f, firstAssertValue, f);
    }
    assert(typeof f, secondAssertValue);
}

function boo(withScope, firstAssertValue,  secondAssertValue) {
    with (withScope) {
        eval(" for(var i = 0; i < 10000; i++ ){ if (i > 0) { function f() { }; } } ");
        assert(typeof f, firstAssertValue);
    }
    assert(typeof f, secondAssertValue);
}
{ 
    for (var i = 0; i < 10000; i++) {
        foo({}, 'function', 'function');
        assertThrow(() => f, "ReferenceError: Can't find variable: f");
    }

    boo({}, 'function', 'function');
}
{
    for (var i = 0; i < 10000; i++) {
        foo({f : 10}, 'number', 'function');
        assertThrow(() => f, "ReferenceError: Can't find variable: f");
    }
    boo({f : 10}, 'number', 'function');

    for (var i = 0; i < 10000; i++) {
        foo({f : {}}, 'object', 'function');
        assertThrow(() => f, "ReferenceError: Can't find variable: f");
    }
    boo({f : {}}, 'object', 'function');
}
{
    for (var i = 0; i < 10000; i++) {
        foo(12345, 'function', 'function');
        assertThrow(() => f, "ReferenceError: Can't find variable: f");
    }
    boo(12345, 'function', 'function');

    for (var i = 0; i < 10000; i++) {
        let val  = 12345;
        val.f = 10;
        foo(val, 'function', 'function');
        assertThrow(() => f, "ReferenceError: Can't find variable: f");
    }
    let x  = 12345;
    x.f = 10;
    boo(x, 'function', 'function');
}
{

    for (var i = 0; i < 10000; i++) {
        foo('12345', 'function', 'function');
        assertThrow(() => f, "ReferenceError: Can't find variable: f");
    }
    boo('12345', 'function', 'function');

    for (var i = 0; i < 10000; i++) {
        let val  = '12345';
        val.f = 10;
        foo(val, 'function', 'function');
        assertThrow(() => f, "ReferenceError: Can't find variable: f");
    }
    let z  = '12345';
    z.f = 10;
    boo(z, 'function', 'function');
}
{
    for (var i = 0; i < 10000; i++) {
        foo(function () {}, 'function', 'function');
        assertThrow(() => f, "ReferenceError: Can't find variable: f");
    }

    boo(function () {}, 'function', 'function');

    for (var i = 0; i < 10000; i++) {
        let val2 = function () {};
        val2.f = 10;
        foo(val2, 'number', 'function');
        assertThrow(() => f, "ReferenceError: Can't find variable: f");
    }

    let val3 = function () {};
    val3.f = 10;
    boo(val3, 'number', 'function');
}