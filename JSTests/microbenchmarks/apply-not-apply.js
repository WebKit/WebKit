"use strict";

(function() {
    let o = {
        apply(a, b) {
            return a + b;
        }
    };
    
    function foo() {
        let result = 0;
        for (let i = 0; i < 1000; ++i)
            result = o.apply(result, 1);
        return result;
    }
    
    noInline(foo);
    
    let result = 0;
    for (let i = 0; i < 10000; ++i)
        result += foo();
    
    if (result != 10000000)
        throw new "Bad result: " + result;
})();

