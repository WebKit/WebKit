function bar(o) {
    if (o.f)
        return o.f;
    else
        return {e:41, f:42};
}

function foo(o) {
    var c = bar(o);
    return c.f;
}

noInline(foo);

// Warm up foo with some different object types.
for (var i = 0; i < 10000; ++i) {
    foo({f:{k:0, f:1}});
    foo({g:1, f:{l: -1, f:2, g:3}});
    foo({h:2, f:null});
}

