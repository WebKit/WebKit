function assert(b) {
    if (!b)
        throw new Error("Bad assertion.")
}

function foo(o) {
    assert(o.length === o.length);
    return o.length;
}
noInline(foo);

let items = [];
const numItems = 30;
for (let i = 0; i < numItems; i++) {
    let o = {};
    for (let j = 0; j < i; j++) {
        o[j + "j"] = j;
    }
    o.length = 2;
    items.push(o);
} 

items.push("st");

for (let i = 0; i < 10000; i++)
    assert(foo(items[i % items.length]) === 2);

Number.prototype.length = 2;
items.push(42);

for (let i = 0; i < 100000; i++)
    assert(foo(items[i % items.length]) === 2);
