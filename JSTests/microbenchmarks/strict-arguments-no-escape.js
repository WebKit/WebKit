"use strict";

function foo()
{
}

noInline(foo);

function bar(o)
{
    foo();
    return o.f.f.f.f.f;
}

function baz()
{
    for (var i = 0; i < 100; ++i) {
        if (bar({f: {f: {f: {f: {f: 42}}}}}) != 42)
            throw "Error: bad result: " + result;
    }
}

noInline(baz);

for (var i = 0; i < 20000; ++i)
    baz();
