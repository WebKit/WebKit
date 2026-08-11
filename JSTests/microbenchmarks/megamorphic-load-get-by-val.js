function f(arr, keys) {
    var res = 0;
    for (var i=0; i<10000000; i++) {
        var o = arr[i % arr.length];
        if (o[keys[i & 0x7]])
            res = 1;
    }
    return res;
}
noInline(f);

var arr = [];
var keys = ['someprop0', 'someprop1', 'someprop2', 'someprop3', 'someprop4', 'someprop5', 'someprop6', 'someprop7' ];

for (var i=0; i<40; i++) {
    var o = {};
    o.someprop0 = 42;
    o.someprop1 = 42;
    o.someprop2 = 42;
    o.someprop3 = 42;
    o.someprop4 = 42;
    o.someprop5 = 42;
    o.someprop6 = 42;
    o.someprop7 = 42;
    o["a" + i] = i;
    arr.push(o);
}

f(arr, keys);
