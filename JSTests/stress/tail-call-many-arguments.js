"use strict";

function foo(...args) {
    return args;
}
noInline(foo);

function bar(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, a30, a31, a32, a33, a34, a35)
{
    return foo(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, a30, a31, a32, a33, a34, a35);
}
noInline(bar);

let args = [];
for (let i = 0; i < 35; ++i) {
    args.push(i);
}

for (let i = 0; i < 100000; ++i) {
    bar(...args);
}
