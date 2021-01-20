let warm = 1000;

function foo(f) {
    return f.caller;
}
noInline(foo);

function bar() {
    for (let i = 0; i < warm; ++i)
        foo(bar);
}
function baz() {
    "use strict";
    foo(baz);
}

bar();

let caught = false;

try {
    baz();
} catch (e) {
    caught = true;
}

if (!caught)
    throw new Error(`bad!`);
