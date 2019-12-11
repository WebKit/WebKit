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

