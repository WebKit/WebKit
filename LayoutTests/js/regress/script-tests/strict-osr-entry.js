"use strict";

(function() {
    let o = {
        apply_(a, b) {
            return a + b;
        }
    };
    
    let result = 0;
    for (let i = 0; i < 10000000; ++i)
        result = o.apply_(result, 1);
    
    if (result != 10000000)
        throw new "Bad result: " + result;
})();

