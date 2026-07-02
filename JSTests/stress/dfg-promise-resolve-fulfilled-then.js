function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(input)
{
    return Promise.resolve(input);
}

class DerivedPromise extends Promise {
    then(a, b) {
        return super.then(a, b);
    }
}

async function testMain()
{
    for (let i = 0; i < testLoopCount; ++i) {
        let promise = new DerivedPromise((resolve, reject) => resolve(undefined));
        let result = await test(promise);
        shouldBe(promise !== result, true);
        let res = await result;
        shouldBe(res, undefined);
    }
}
testMain().catch($vm.abort);
