function assert(x) {
    if (!x)
        throw new Error("Bad assertion!");
}

const sym = Symbol("foo");
const obj = {};
obj[sym] = 1;
const parsed = JSON.parse('{"foo": 42}');
assert(!(parsed[sym] === 42 && parsed.foo === undefined));
