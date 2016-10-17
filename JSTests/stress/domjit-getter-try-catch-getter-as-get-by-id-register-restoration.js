function assert(b) {
    if (!b) throw new Error("bad value");
}
noInline(assert);

let i;
var o1 = createDOMJITGetterComplexObject();
o1.x = "x";

var o2 = {
    customGetter: 40
}

var o3 = {
    x: 100,
    customGetter: "f"
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
    let v;
    let v2;
    let v3;
    try {
        v2 = o.x;
        v = o.customGetter + v2;
    } catch(e) {
        assert(v2 === "x");
        assert(o === o1);
    }
}
noInline(foo);

foo(i);
for (i = 0; i < 1000; i++)
    foo(i);

o1.enableException();
i = -1000;
for (let j = 0; j < 1000; j++)
    foo(i);
