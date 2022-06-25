function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

shouldBe(JSON.stringify(Object.getOwnPropertyDescriptor(Object, "fromEntries")), `{"writable":true,"enumerable":false,"configurable":true}`);
shouldBe(Object.fromEntries.length, 1);

shouldThrow(() => Object.fromEntries(null), `TypeError: null is not an object`);
shouldThrow(() => Object.fromEntries(undefined), `TypeError: undefined is not an object`);
shouldThrow(() => Object.fromEntries(0), `TypeError: undefined is not a function`);
shouldThrow(() => Object.fromEntries(true), `TypeError: undefined is not a function`);
shouldThrow(() => Object.fromEntries(Symbol("Cocoa")), `TypeError: undefined is not a function`);
shouldThrow(() => Object.fromEntries("Cocoa"), `TypeError: Object.fromEntries requires the first iterable parameter yields objects`);
shouldThrow(() => Object.fromEntries([0]), `TypeError: Object.fromEntries requires the first iterable parameter yields objects`);
shouldThrow(() => Object.fromEntries([["Cocoa", "Cappuccino"], 0]), `TypeError: Object.fromEntries requires the first iterable parameter yields objects`);

{
    let object = Object.fromEntries([]);
    shouldBe(JSON.stringify(object), `{}`);
}
{
    let object = Object.fromEntries([["Cocoa", "Cappuccino"]]);
    shouldBe(JSON.stringify(object), `{"Cocoa":"Cappuccino"}`);
    shouldBe(JSON.stringify(Object.getOwnPropertyDescriptor(object, "Cocoa")), `{"value":"Cappuccino","writable":true,"enumerable":true,"configurable":true}`);
}
{
    let obj = { abc: 1, def: 2, ghij: 3 };
    let res = Object.fromEntries(
        Object.entries(obj)
            .filter(([ key, val ]) => key.length === 3)
            .map(([ key, val ]) => [ key, val * 2 ])
        );
    shouldBe(JSON.stringify(res), `{"abc":2,"def":4}`);
}
{
    let map = new Map([ [ 'a', 1 ], [ 'b', 2 ], [ 'c', 3 ] ]);
    let obj = Object.fromEntries(map);
    shouldBe(JSON.stringify(obj), `{"a":1,"b":2,"c":3}`);
}
{
    let arr = [ { name: 'Alice', age: 40 }, { name: 'Bob', age: 36 } ];
    let obj = Object.fromEntries(arr.map(({ name, age }) => [ name, age ]));
    shouldBe(JSON.stringify(obj), `{"Alice":40,"Bob":36}`);
}
{
    Object.defineProperty(Object.prototype, "bad", {
        get() { throw new Error("out"); },
        set(v) { throw new Error("out"); }
    });
    shouldThrow(() => {
        let object = {};
        object.bad;
    }, `Error: out`);
    shouldThrow(() => {
        let object = {};
        object.bad = 42;
    }, `Error: out`);
    let object = Object.fromEntries([["bad", "value"]]);
    shouldBe(JSON.stringify(object), `{"bad":"value"}`);
}
{
    var counter = 0;
    class Recorder {
        constructor(first, second)
        {
            this.first = first;
            this.second = second;
        }

        get 0()
        {
            shouldBe(counter++, this.first);
            return this.first;
        }

        get 1()
        {
            shouldBe(counter++, this.second);
            return this.second;
        }
    }
    var result = Object.fromEntries([new Recorder(0, 1), new Recorder(2, 3)]);
    shouldBe(result[0], 1);
    shouldBe(result[2], 3);
    shouldBe(counter, 4);
}
{
    class Iterable {
        constructor()
        {
        }

        *[Symbol.iterator]()
        {
            yield [0, 1];
            yield [1, 2];
        }
    }

    var result = Object.fromEntries(new Iterable);
    shouldBe(result[0], 1);
    shouldBe(result[1], 2);
}
{
    class Iterator {
        constructor()
        {
            this.index = 0;
        }

        next()
        {
            if (this.index === 4)
                throw new Error("out");

            this.index++;
            return {
                value: [0, 1],
                done: false
            };
        }
    }

    class Iterable {
        constructor()
        {
        }

        [Symbol.iterator]()
        {
            return new Iterator;
        }
    }

    try {
        Object.fromEntries(new Iterable);
    } catch (error) {
        shouldBe(String(error), `Error: out`);
    }
}
{
    let array = [[], ['c', 'd']];
    let object = Object.fromEntries(array);
    shouldBe(JSON.stringify(Object.keys(object).sort()), `["c","undefined"]`);
    shouldBe(object.c, 'd');
    shouldBe(object.undefined, undefined);
}
{
    let symbol = Symbol('Cocoa');
    let array = [[symbol, 42]];
    let object = Object.fromEntries(array);
    shouldBe(Object.getOwnPropertySymbols(object).length, 1);
    shouldBe(Object.getOwnPropertyNames(object).length, 0);
    shouldBe(object.hasOwnProperty(symbol), true);
    shouldBe(object[symbol], 42);
}
{
    Object.defineProperty(Object.prototype, "hello", {
        get() {
            throw new Error("out");
        },
        set() {
            throw new Error("out");
        }
    });
    let result = Object.fromEntries([["hello", 42]]);
    shouldBe(result.hello, 42);
}
{
    let array = [['a', 'b'], ['c', 'd']];
    Object.defineProperty(array, 0, {
        get()
        {
            array.push(['e', 'f']);
            return ['a', 'b'];
        }
    });
    let object = Object.fromEntries(array);
    shouldBe(JSON.stringify(object), `{"a":"b","c":"d","e":"f"}`);
}
