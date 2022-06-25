function* gen() { yield; }
let foo = gen();
let obj = {};
obj.__proto__ = foo;

try {
    obj.next().value;
    throw "bad";
} catch (e) {
    if (!(e instanceof TypeError))
        throw e;
}
