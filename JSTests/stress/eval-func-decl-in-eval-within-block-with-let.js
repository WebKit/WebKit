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

function foo() {
    {
        let f = 20;
        eval(" { function f() { value = 20; }; }");
        assert(f, 20);
    }
    assertThrow(() => f, "ReferenceError: Can't find variable: f");
}


for (var i = 0; i < 10000; i++){
    foo();
    assertThrow(() => f, "ReferenceError: Can't find variable: f");
}

function boo() {
    {
        var l = 20;
        eval(" { function l() { value = 20; }; }");
        assert(typeof l, 'function');
    }
    assert(typeof l, 'function');
}

for (var i = 0; i < 10000; i++){
    boo();
    assertThrow(() => l, "ReferenceError: Can't find variable: l");
}

function goo() {
    {
        let g = 20;
        eval(" for(var j=0; j < 10000; j++){ function g() { }; } ");
        assert(typeof g, 'number');
    }
    assertThrow(() => g, "ReferenceError: Can't find variable: g");
}

goo();
assertThrow(() => g, "ReferenceError: Can't find variable: g");
