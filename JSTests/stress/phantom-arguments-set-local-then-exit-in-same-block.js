function foo(o) {
    var a = arguments;
    var result = o.f;
    for (var i = 1; i < a.length; ++i)
        result += a[i];
    return result;
}

noInline(foo);

for (var i = 0; i < 100; ++i)
    foo({f:42}, 1, 2, 3);

var result = foo({g:40, f:41}, 1, 2.5, 3);
if (result != 47.5)
    throw "Bad result: " + result;

