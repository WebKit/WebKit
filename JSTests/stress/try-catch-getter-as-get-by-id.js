function assert(b) {
    if (!b) throw new Error("b");
}
noInline(assert);


let i;
var o1 = { 
    get f() {
        if (i === -1000)
            throw new Error("hello");
        return 20;
    }
};

var o2 = {
    f: 40
}

var o3 = { 
    x: 100,
    f: 50 
}

function bar(i) {
    if (i === -1000)
        return o1;

    if (i % 2)
        return o3;
    else
        return o2;
}
noInline(bar);

function foo(i) {
    var o = bar(i);
    var v;
    try {
        v = o.f
    } catch(e) {
        assert(o === o1);
    }
}
noInline(foo);

foo(i);
for (i = 0; i < 1000; i++)
    foo(i);

i = -1000;
for (let j = 0; j < 1000; j++)
    foo(i);
