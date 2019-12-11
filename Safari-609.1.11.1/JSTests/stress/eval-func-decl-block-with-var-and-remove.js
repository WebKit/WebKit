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

function foo() {
    {   
        assertThrow(() => f, "ReferenceError: Can't find variable: f");
        eval('eval(" { function f() { }; } ")');
        assert(typeof f, "function");
    }
    assert(typeof f, "function", "#1");
    delete f;
    assertThrow(() => f, "ReferenceError: Can't find variable: f", "#1");
}

for (var i = 0; i < 10000; i++) {
    foo();
    assertThrow(() => f, "ReferenceError: Can't find variable: f");
}

function boo() {
    {
        assert(typeof l, "undefined", "#5");
        eval('{ var l = 15; eval(" { function l() { }; } ")}');
        assert(typeof l, "function", "#3");
    }
    assert(typeof l, 'function', "#4");
    delete l;
    assertThrow(() => f, "ReferenceError: Can't find variable: f");
}

for (var i = 0; i < 10000; i++){
    boo();
    assertThrow(() => f, "ReferenceError: Can't find variable: f");
}

function joo() {
    {
        assert(typeof h, "undefined" );
        eval('eval(" if (true){ function h() { }; } ")');
        assert(typeof h, "function" );
    }
    assert(typeof h, "function", "#10");
    delete h;
    assertThrow(() => h, "ReferenceError: Can't find variable: h");
}

for (var i = 0; i < 10000; i++){
    joo();
    assertThrow(() => h, "ReferenceError: Can't find variable: h");
}

function koo() {
    {
        var k = 20;
        eval('var k = 15; eval(" if (true){ function k() { }; } ")');
        assert(typeof k, "function" );
    }
    assert(typeof k, "function", "#12");
    delete k;
    assert(typeof k, "function", "#12");
}

for (var i = 0; i < 10000; i++){
    koo();
    assertThrow(() => k, "ReferenceError: Can't find variable: k");
}
