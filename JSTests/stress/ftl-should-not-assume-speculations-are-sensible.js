let a3 = {z: 1000};
let a1 = [];
a1[0] = 0;

function foo(x, y) {
    'use strict';
    for (let i = 0; i < 20; ++i) {
        for (let j = 0; j < x.z; ++j) {
            foo(0, i);
            y[0] = undefined;
        }
    }
}

foo(a3, []);
for (let i = 0; i < 2; ++i) {
    foo(a3, a1);
}
