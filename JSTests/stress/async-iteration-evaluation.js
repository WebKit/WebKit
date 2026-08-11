var assert = function (result, expected, message = "") {
  if (result !== expected) {
    throw new Error('Error in assert. Expected "' + expected + '" but was "' + result + '":' + message );
  }
};

let error = false; 

async function * foo(value = obj) {
    yield '1';
    return 'end';
}

try {
    var f = foo();
} catch(e) {
    error = e instanceof ReferenceError;
}

assert(error, true, 'Error in arguments declaration should to error during evaluation of async generator.');