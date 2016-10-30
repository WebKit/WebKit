function foo(o) {
    if (o.f)
        return o.f + o.f + o.f + o.f; 
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

for (let i = 0; i < 10000000; i++) {
    let obj = objects[i % objects.length];
    foo(obj);
}

const verbose = false;
if (verbose)
    print(Date.now() - start);
