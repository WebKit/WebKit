function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    let object = {
        hello: {
            get inner() {
                $vm.toUncacheableDictionary(object);
                object.world = 0;
                return 33;
            }
        },
        world: 42,
        test: null,
    };

    shouldBe(JSON.stringify(object), `{"hello":{"inner":33},"world":0,"test":null}`);
}
{
    let object = {
        hello: {
            get inner() {
                delete object.world;
                object.testing = 33;
                return 33;
            }
        },
        world: 42,
        test: null,
    };

    shouldBe(JSON.stringify(object), `{"hello":{"inner":33},"test":null}`);
}
