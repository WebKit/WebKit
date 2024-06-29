function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`expected ${b} but got ${a}`);
}

const runs = 1e5;
const regExpPrototype = RegExp.prototype;

function test(propertyName) {
    let count = 0;
    let message = null;
    for (let i = 0; i < runs; i++) {
        try {
            regExpPrototype[propertyName]();
        } catch (error) {
            if (message === null)
                message = error.message;
            if (error.message === message)
                count++;
        }
    }
    shouldBe(count, runs);
}

noInline(test);

test(Symbol.match);
test(Symbol.replace);
test(Symbol.search);
test("test");
