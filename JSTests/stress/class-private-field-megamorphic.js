function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(i)
{
    class A {
        #field = 0;
        get()
        {
            return this.#field;
        }
        put(i)
        {
            this.#field = i;
        }
    }

    let instance = new A;
    instance.put(i);
    return instance.get();
}

for (var i = 0; i < 1e5; ++i)
    shouldBe(test(i), i);
