"use strict";

(function() {
    let o = {
        call(ignored, a, b) {
            return a + b;
        }
    };
    
    let a = [o, (a, b) => a - 2 * b];
    
    function foo() {
        let result = 0;
        for (let i = 0; i < 1000; ++i)
            result = a[((i % 5) == 0) | 0].call(null, result, 1);
        return result;
    }
    
    noInline(foo);
    
    let result = 0;
    for (let i = 0; i < 10000; ++i)
        result += foo();
    
    if (result != 4000000)
        throw "Bad result: " + result;
})();

