function test(o) {
    let sum = 0;
    for (let i in o)
        sum += o[i];
    return sum;
}
noInline(test);

Object.defineProperty(Object.prototype, "foo", { enumerable: true, value: 4 });

class Foo extends Array {
    b = 1;
}

let object = new Foo();
let object2 = new Foo();
object2.length = 100;
object2.fill(1);

for (let i = 0; i < 1e5; ++i) {
    let sum = test(object);
    if (sum !== 5)
        throw new Error(sum);
}

let sum = test(object2);
if (sum !== 105)
    throw new Error(sum);
