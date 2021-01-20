'use strict';

var count = 0;
function foo() {
    count--;
    if (count === 0)
        return 30;
    return foo(42, 42);
}

function test() {
    count = 100;
    return foo(...[42, 42]);
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    test();
