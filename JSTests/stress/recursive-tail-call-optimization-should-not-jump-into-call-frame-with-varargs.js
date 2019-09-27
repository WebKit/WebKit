'use strict';
var o;
function foo() {
    return o.baz();
}

class C1 {
    baz() {
    }
};
var x = new C1();

function bar() {
    o = x;
};
noInline(bar)

function goo() {
    return foo([[]], bar());
}

class C2 {
    baz() {
        return goo();
    }
};
var y = new C2();

function test() {
    o = y;
    return foo(...[43646, 43754]);
}
noInline(test)

var result = 0;
for (var i = 0; i < 1000000; ++i)
    result += test();
