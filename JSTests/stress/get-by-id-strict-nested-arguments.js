let warm = 1000;

function foo(f) {
    return f.arguments;
}
noInline(foo);

let caught = false;

function bar() {
    for (let i = 0; i < warm; ++i)
        foo(bar);
    function baz() {
        "use strict";
        try {
            foo(baz);
        } catch (e) {
            caught = true;
        }
    }
    baz();
}

bar();

if (!caught)
    throw new Error(`bad!`);
