function foo() {
    "use strict";
    let a = "hello" + a;
    return a;
}

noInline(foo);

for (var i = 0; i < testLoopCount; ++i) {
    try {
        foo();
    } catch (e) {
    }
}
