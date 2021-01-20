// Regression test for https://webkit.org/b/156765. This test should not hang.

"use strict";

let forty = 40;

function realValue()
{
    return forty + 2;
}

var object = {
    get hello() {
        return realValue();
    }
};

function ok() {
    var value = 'hello';
    if (object[value] + 20 !== 62)
        throw new Error();
}

noInline(ok);

for (var i = 0; i < 100000; ++i)
    ok();
