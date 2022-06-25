//@ runDefault

"use strict";

let x;

let o = {
    set foo(value)
    {
        x = value;
    }
};

Object.freeze(o);

o.foo = 42;

if (x != 42)
    throw "Error: bad result: " + x;

