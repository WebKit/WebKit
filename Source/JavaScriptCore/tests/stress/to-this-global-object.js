function test() {
    return this.f;
}
noInline(test);

function test2() {
    "use strict";
    return this.f;
}
noInline(test2);

f = 42;

let get = eval;
let global = get("this");

for (var i = 0; i < 10000; ++i) {
    let result = test.call(global);
    if (result !== 42)
        throw new Error("bad this value: " + result);

    result = test2.call(global);
    if (result !== 42)
        throw new Error("bad this value: " + result);
}
