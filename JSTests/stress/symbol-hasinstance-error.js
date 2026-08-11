function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

let a = function() {};
Object.defineProperty(a, Symbol.hasInstance, { value: Atomics });
let b = a.bind();
shouldThrow(() => {
    Function.prototype[Symbol.hasInstance].call(b)
}, `TypeError: function [Symbol.hasInstance] is not a function, undefined, or null`);
