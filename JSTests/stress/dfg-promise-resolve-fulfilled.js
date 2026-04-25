function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(input)
{
    return Promise.resolve(input);
}

async function testMain()
{
    for (let i = 0; i < testLoopCount; ++i) {
        let res = await test(undefined);
        shouldBe(res, undefined);
    }
}
testMain().catch($vm.abort);
