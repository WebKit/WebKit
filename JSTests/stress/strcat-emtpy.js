function foo() {
    "use strict";
    let a = "hello" + a;
    return a;
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    try {
        foo();
    } catch (e) {
    }
}
