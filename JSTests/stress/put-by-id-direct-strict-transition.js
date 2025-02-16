"use strict"

let theglobal = 0;
for (theglobal = 0; theglobal < testLoopCount; ++theglobal)
    ;
const foo = (ignored, arg1) => { theglobal = arg1; };
for (let j = 0; j < testLoopCount; ++j) {
    const obj = {
        set hello(ignored) {},
        [theglobal]: 0
    };
    foo(obj, 'hello');
}
