function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    let name = 'prototype';
    let object = {
        prototype() { },
        get [name]() { },
    };

    shouldBe(JSON.stringify(Object.getOwnPropertyDescriptor(object, 'prototype')), `{"enumerable":true,"configurable":true}`);
}

{
    let name = 'prototype';
    let object = {
        get [name]() { },
        prototype() { },
    };

    shouldBe(JSON.stringify(Object.getOwnPropertyDescriptor(object, 'prototype')), `{"writable":true,"enumerable":true,"configurable":true}`);
}


{
    let name = 'prototype';
    let object = {
        [name]() { },
        get prototype() { },
    };

    shouldBe(JSON.stringify(Object.getOwnPropertyDescriptor(object, 'prototype')), `{"enumerable":true,"configurable":true}`);
}

{
    let name = 'prototype';
    let object = {
        get prototype() { },
        [name]() { },
    };

    shouldBe(JSON.stringify(Object.getOwnPropertyDescriptor(object, 'prototype')), `{"writable":true,"enumerable":true,"configurable":true}`);
}

{
    let object = {
        __proto__() { }
    };
    shouldBe(JSON.stringify(Object.getOwnPropertyDescriptor(object, '__proto__')), `{"writable":true,"enumerable":true,"configurable":true}`);
    shouldBe(Object.getPrototypeOf(object), Object.prototype);
}

{
    let name = '__proto__';
    let object = {
        [name]() { }
    };
    shouldBe(JSON.stringify(Object.getOwnPropertyDescriptor(object, '__proto__')), `{"writable":true,"enumerable":true,"configurable":true}`);
    shouldBe(Object.getPrototypeOf(object), Object.prototype);
}

{
    let name = '42';
    let object = {
        42() { },
        get [name]() { },
    };

    shouldBe(JSON.stringify(Object.getOwnPropertyDescriptor(object, '42')), `{"enumerable":true,"configurable":true}`);
}

{
    let name = '42';
    let object = {
        get [name]() { },
        42() { },
    };

    shouldBe(JSON.stringify(Object.getOwnPropertyDescriptor(object, '42')), `{"writable":true,"enumerable":true,"configurable":true}`);
}


{
    let name = '42';
    let object = {
        [name]() { },
        get 42() { },
    };

    shouldBe(JSON.stringify(Object.getOwnPropertyDescriptor(object, '42')), `{"enumerable":true,"configurable":true}`);
}

{
    let name = '42';
    let object = {
        get 42() { },
        [name]() { },
    };

    shouldBe(JSON.stringify(Object.getOwnPropertyDescriptor(object, '42')), `{"writable":true,"enumerable":true,"configurable":true}`);
}


