let o = {
    set foo(x) {
        'use strict';
        return bar();
    }
};

function bar() {
    return 20;
}

for (let i = 0; i < testLoopCount; i++)
    o.foo = 20;
