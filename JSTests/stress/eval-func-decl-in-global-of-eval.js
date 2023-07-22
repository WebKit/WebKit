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
}


function bar() {
    {
        let f = 20;
        let value = 10; 
        eval("function f() { value = 20; }; f();");
    }
}


for (var i = 0; i < 10000; i++){
    assertThrow(() => bar(), "SyntaxError: Can't create duplicate variable in eval: 'f'");
    assertThrow(() => f, "ReferenceError: Can't find variable: f");
}

function baz() {
    {
        var l = 20;
        var value = 10;
        eval("function l() { value = 20; }; l();");
        assert(typeof l, 'function');
        assert(value, 20);
    }
    assert(typeof l, 'function');
}

for (var i = 0; i < 10000; i++){
    baz();
    assertThrow(() => l, "ReferenceError: Can't find variable: l");
}

function foobar() {
    {
        let g = 20;
        let value = 10;
        eval("function l() { value = 30; }; l();");
        assert(typeof g, 'number');
        assert(value, 30);
    }
    assertThrow(() => g, "ReferenceError: Can't find variable: g");
}

foobar();
assertThrow(() => g, "ReferenceError: Can't find variable: g");

(function() {
    try {
        let b;
        let c;
        eval('var a; var b; var c;');
    } catch (e) {
        var error = e;
    }

    assert(error.toString(), "SyntaxError: Can't create duplicate variable in eval: 'c'");
    assertThrow(() => a, "ReferenceError: Can't find variable: a");
    assertThrow(() => b, "ReferenceError: Can't find variable: b");
    assertThrow(() => c, "ReferenceError: Can't find variable: c");
})();

(function() {
    try {
        let x1;
        eval('function x1() {} function x2() {} function x3() {}');
    } catch (e) {
        var error = e;
    }

    assert(error.toString(), "SyntaxError: Can't create duplicate variable in eval: 'x1'");
    assertThrow(() => x1, "ReferenceError: Can't find variable: x1");
    assertThrow(() => x2, "ReferenceError: Can't find variable: x2");
    assertThrow(() => x3, "ReferenceError: Can't find variable: x3");
})();

(function() {
    var x3;
    try {
        let x2;
        eval('function x1() {} function x2() {} function x3() {}');
    } catch (e) {
        var error = e;
    }

    assert(error.toString(), "SyntaxError: Can't create duplicate variable in eval: 'x2'");
    assertThrow(() => x1, "ReferenceError: Can't find variable: x1");
    assertThrow(() => x2, "ReferenceError: Can't find variable: x2");
    assert(x3, undefined);
})();
