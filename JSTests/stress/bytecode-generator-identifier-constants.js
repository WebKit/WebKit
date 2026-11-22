//@ runDefault
//@ runBytecodeCache

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: expected ${expected} but got ${actual}`);
}

// Written out in the file so that the bytecode cache variant encodes and decodes these code blocks.
function cached(o) {
    o.a = 1; o.b = 2; o.c = 3; o.d = 4; o.e = 5; o.f = 6; o.g = 7; o.h = 8; o.i = 9; o.j = 10;
    o.a += 100; o.b += 100;
    return o.a + o.b + o.c + o.d + o.e + o.f + o.g + o.h + o.i + o.j;
}
shouldBe(cached({}), 255);

// Private methods use a symbol (the private brand name) as an identifier constant.
class WithPrivateMethods {
    #first() { return 1; }
    static #second() { return 2; }
    sum() { return this.#first() + WithPrivateMethods.#second() + this.#first() + WithPrivateMethods.#second(); }
}
shouldBe(new WithPrivateMethods().sum(), 6);

// Enough names to grow the identifier table several times, each looked up again after the growth.
const names = [];
for (let i = 0; i < 40; ++i)
    names.push(`property${i}`);
const stores = names.map((name, i) => `o.${name} = ${i};`).join("\n");
const loads = names.map(name => `sum += o.${name};`).reverse().join("\n");
const expected = 780;

const object = {};
shouldBe(new Function("o", `${stores}\nlet sum = 0;\n${loads}\nreturn sum;`)(object), expected);
shouldBe(Object.keys(object).join(), names.join());

// Generatorification appends identifiers after the ones that the table knows about.
const iterator = (0, eval)(`(function* (o) {\n${stores}\nyield o;\nlet sum = 0;\n${loads}\nreturn sum;\n})`)({});
iterator.next();
gc();
shouldBe(iterator.next().value, expected);
