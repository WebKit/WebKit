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
    let result = Object.assign({}, source);
    shouldBe(result.x, 42);
    shouldBe(result.hasOwnProperty('y'), false);
}

{
    let source = {
        get x() {
            return 42;
        },
        y: 42
    };
    var store = 0;
    let target = {
        set x(value) {
            store = value;
            delete source.y;
        },
        get x() {
            return store;
        },
    };
    let result = Object.assign(target, source);
    shouldBe(result.x, 42);
    shouldBe(result.hasOwnProperty('y'), false);
}


{
    let source = {
        get x() {
            Object.defineProperty(source, 'y', {
                enumerable: false
            });
            return 42;
        },
        y: 42
    };
    let result = Object.assign({}, source);
    shouldBe(result.x, 42);
    shouldBe(result.hasOwnProperty('y'), false);
}

{
    let source = {
        get x() {
            return 42;
        },
        y: 42
    };
    var store = 0;
    let target = {
        set x(value) {
            store = value;
            Object.defineProperty(source, 'y', {
                enumerable: false
            });
        },
        get x() {
            return store;
        },
    };
    let result = Object.assign(target, source);
    shouldBe(result.x, 42);
    shouldBe(result.hasOwnProperty('y'), false);
}
