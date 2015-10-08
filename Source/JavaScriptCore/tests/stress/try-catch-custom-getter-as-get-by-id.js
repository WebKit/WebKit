function assert(b) {
    if (!b) throw new Error("b");
}
noInline(assert);

let i;
var o1 = createCustomGetterObject();
o1.shouldThrow = false;

var o2 = {
    customGetter: 40
}

var o3 = { 
    x: 100,
    customGetter: 50 
}

i = -1000;
bar(i);
foo(i);
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
        v = o.customGetter;
    } catch(e) {
        assert(o === o1);
    }
}
noInline(foo);

foo(i);
for (i = 0; i < 1000; i++)
    foo(i);

i = -1000;
for (let j = 0; j < 1000; j++) {
    if (j > 10)
        o1.shouldThrow = true;
    foo(i);
}
