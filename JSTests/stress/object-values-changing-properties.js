function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    let source = {
        get x() {
            delete this.y;
            return 42;
        },
        y: 42
    };
    let result = Object.values(source);
    shouldBe(result.length, 1);
    shouldBe(result[0], 42);
}

{
    let source = Object.defineProperties({}, {
        nonEnumerable: {
            enumerable: false,
            value: 42
        }
    });

    let result = Object.values(source);
    shouldBe(result.length, 0);
}

{
    let order = [];
    let target = {x: 20, y:42};
    let handler = {
        getOwnPropertyDescriptor(theTarget, propName)
        {
            order.push(`getOwnPropertyDescriptor ${propName}`);
            return {
                enumerable: true,
                configurable: true,
                value: 42
            };
        },
        get(theTarget, propName, receiver)
        {
            order.push(`get ${propName}`);
            return 20;
        }
    };

    let proxy = new Proxy(target, handler);
    let result = Object.values(proxy);
    shouldBe(result.length, 2);
    shouldBe(result[0], 20);
    shouldBe(result[1], 20);
    shouldBe(order.join(','), `getOwnPropertyDescriptor x,get x,getOwnPropertyDescriptor y,get y`);
}

{
    let order = [];
    let target = Object.defineProperties({}, {
        x: {
            enumerable: false,
            configurable: true,
            value: 20
        },
        y: {
            enumerable: false,
            configurable: true,
            value: 42
        }
    });

    let handler = {
        getOwnPropertyDescriptor(theTarget, propName)
        {
            order.push(`getOwnPropertyDescriptor ${propName}`);
            return {
                enumerable: false,
                configurable: true,
                value: 42
            };
        },
        get(theTarget, propName, receiver)
        {
            order.push(`get ${propName}`);
            return 42;
        }
    };

    let proxy = new Proxy(target, handler);
    let result = Object.values(proxy);
    shouldBe(result.length, 0);
    shouldBe(order.join(','), `getOwnPropertyDescriptor x,getOwnPropertyDescriptor y`);
}
