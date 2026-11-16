function test(o) {
    let result = o.a - o.b;
    return result < 5 ? 1 : 2;
}
noInline(test);

let objects = [];
for (let i = 0; i < 16; ++i)
    objects.push({ a: (i & 7) ? i : undefined, b: 3 });

let result = 0;
for (let i = 0; i < 4e7; ++i)
    result += test(objects[i & 15]);

if (result !== 62500000)
    throw new Error("bad result: " + result);
