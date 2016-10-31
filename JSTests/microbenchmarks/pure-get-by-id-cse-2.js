function foo(o, c) {
    if (o.f) {
        let sum = 0;
        for (let i = 0; i < c; i++)
            sum += o.f;
        return sum;
    }
}
noInline(foo);

let start = Date.now();
let objects = [];
const objectCount = 20;
for (let i = 0; i < objectCount; i++) {
    let obj = {};
    for (let j = 0; j < i * 2; j++) {
        obj["j" + j] = j;
    }
    obj.f = 20;
    objects.push(obj);
}

for (let i = 0; i < 10000; i++) {
    let obj = objects[i % objects.length];
    foo(obj, 25);
}

const verbose = false;
if (verbose)
    print(Date.now() - start);
