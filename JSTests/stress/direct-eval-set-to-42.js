function foo() {
    try { eval(""); } catch { }
}
noInline(foo);

function bar() {
    "use strict";
    try { eval(""); } catch { }
}
noInline(bar);

eval = 42;
for (let i = 0; i < 1e5; ++i) {
    foo();
    bar();
}
