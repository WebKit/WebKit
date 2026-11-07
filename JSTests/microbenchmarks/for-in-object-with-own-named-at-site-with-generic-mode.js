function sum(o) {
    let sum = 0;
    for (let p in o)
        sum += o[p];
    return sum;
}
noInline(sum);

let objects = [];
for (let i = 0; i < 16; ++i)
    objects.push({ a: 1, b: 2, c: 3, d: 4, e: 5, f: 6, g: 7, h: 8 });
objects[7] = Object.create({ g: 7, h: 8 });
Object.assign(objects[7], { a: 1, b: 2, c: 3, d: 4, e: 5, f: 6 });

for (let i = 0; i < 1e5; ++i) {
    for (let j = 0; j < objects.length; ++j) {
        if (sum(objects[j]) !== 36)
            throw new Error("bad sum");
    }
}
