function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    var order = [];
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
            return 42;
        }
    };

    let proxy = new Proxy(target, handler);
    var result = Object.assign({}, proxy);
    shouldBe(result.x, 42);
    shouldBe(result.y, 42);
    shouldBe(order.join(','), `getOwnPropertyDescriptor x,get x,getOwnPropertyDescriptor y,get y`);
}
