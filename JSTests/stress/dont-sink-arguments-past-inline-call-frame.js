function foo() {
    'use strict';
    return arguments;
}

function fooWrap() {
    return foo(...arguments);
}

function empty(a0, a1) {
}

function bar() {
    let j = undefined;
    let args = fooWrap();
    0..x = undefined;
    empty(args);
}
noInline(bar);

for (let i = 0; i < 10000; ++i) {
    bar();
}
