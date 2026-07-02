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

var foo = 'foo';
var success = false;
// FIXME: According to spec foo should have value 'undefined', 
// but possible it would be changed https://github.com/tc39/ecma262/issues/753 
eval("success = foo === 'foo'; { function foo(){} }");

assert(success, true);

success = false;

let boo = 'boo';
eval("success = boo === 'boo'; { function boo(){} } success = success && boo === 'boo';");

success = success && boo === 'boo';

assert(success, true);
