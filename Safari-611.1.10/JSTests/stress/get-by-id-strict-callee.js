let warm = 1000;

function bar() {
    for (let i = 0; i < warm; ++i)
        arguments.callee;
}

function baz() {
    "use strict";
    arguments.callee;
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
