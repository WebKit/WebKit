function f(arr) {
    var res = 0;
    for (var i=0; i<10000000; i++) {
        var o = arr[i % arr.length];
        if (o.someprop)
            res = 1;
    }
    return res;
}
noInline(f);

var arr = [];

for (var i=0; i<40; i++) {
    var o = {};
    o["a" + i] = i;
    arr.push(o);
}

f(arr);
