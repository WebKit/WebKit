"use strict"

let theglobal = 0;
for (theglobal = 0; theglobal < 100000; ++theglobal)
    ;
const foo = (ignored, arg1) => { theglobal = arg1; };
for (let j = 0; j < 10000; ++j) {
    const obj = {
        set hello(ignored) {},
        [theglobal]: 0
    };
    foo(obj, 'hello');
}
