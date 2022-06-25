//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
var x = "x";
var y = "y";
var a = "a";
var b = "b";
var c = "c";
var d = "d";
var e = "e";
var f = "f";
var g = "g";
var h = "h";
var i = "i";

function foo(o) {
    o[x] = 1;
    o[y] = 2;
    o[a] = 3;
    o[b] = 4;
    o[c] = 5;
    o[d] = 6;
    o[e] = 7;
    o[f] = 8;
    o[g] = 9;
    o[h] = 10;
    o[i] = 11;
}

function Foo() {
    foo(this);
}

var result = 0;

for (var i = 0; i < 100000; ++i) {
    foo({[f]:42});
    result += (new Foo())[x];
}

if (result != 100000)
    throw "Bad result: " + result;

