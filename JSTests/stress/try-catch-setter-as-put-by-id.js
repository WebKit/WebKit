function assert(b) {
    if (!b) 
        throw new Error("bad assertion");
}
noInline(assert);


let i;
var o1 = { 
    set f(v) {
        if (i === -1000)
            throw new Error("hello");
        this._v = v;
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
    let o = bar(i);
    let v = o.x;
    try {
        o.f = v;
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
