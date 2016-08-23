function foo(a) {
    var result = 0.0;
    for (var i = 0; i < 1000; ++i) {
        var o = a[i];
        result += o.x + o.y;
    }
    return result;
}

noInline(foo);

var array = [];
for (var i = 0; i < 1000; ++i)
    array.push({x:1.5, y:2.5});
for (var i = 0; i < 10000; ++i)
    foo(array);
