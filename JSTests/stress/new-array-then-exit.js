function foo(f) {
    return new f();
}

noInline(foo);

for (var i = 0; i < 10000; ++i)
    foo(Array);

var didCall = false;
foo(function() { didCall = true; });

if (!didCall)
    throw "Error: didn't call my function.";
