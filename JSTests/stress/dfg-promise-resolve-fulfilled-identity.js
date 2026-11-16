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
        let input = new Promise((resolve, reject) => resolve(undefined));
        let result = test(input);
        shouldBe(result, input);
        let res = await result;
        shouldBe(res, undefined);
    }
}
testMain().catch($vm.abort);
