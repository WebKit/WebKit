//@ runNoJIT

function bar(a, idx)
{
    "use strict";
    if (idx > 0)
      throw "Hello";
    return a;
}

boundBar = bar.bind(null, 42);

function foo(a, idx)
{
    "use strict";
    return boundBar(idx);
}

boundFoo = foo.bind(null, 41);

try {
    boundFoo(1);
} catch(e) {}
