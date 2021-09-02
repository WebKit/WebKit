function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(i)
{
    class A {
        #field = 0;
        put(i)
        {
            this.#field = i;
        }
        get()
        {
            return this.#field;
        }
    }
    noInline(A.prototype.get);
    noInline(A.prototype.put);
    return new A;
}

let test0 = test(0);
let test1 = test(1);
let test2 = test(2);
let test3 = test(3);
let test4 = test(4);

for (var i = 0; i < 1e5; ++i) {
    test0.put(i + 0);
    shouldBe(test0.get(), i + 0);
    test1.put(i + 1);
    shouldBe(test1.get(), i + 1);
    test2.put(i + 2);
    shouldBe(test2.get(), i + 2);
    test3.put(i + 3);
    shouldBe(test3.get(), i + 3);
    test4.put(i + 4);
    shouldBe(test4.get(), i + 4);
}
