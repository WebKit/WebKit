function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error(`bad value: expected ${expected} but got ${actual}`);
}

const count = 3000;

async function* generator()
{
    for (let index = 0; index < count; ++index)
        yield index;
}

async function sum()
{
    let result = 0;
    for await (const value of generator())
        result += value;
    return result;
}

asyncTestStart(1);
sum().then((result) => {
    shouldBe(result, count * (count - 1) / 2);
    asyncTestPassed();
});

// Deleting all code detaches every CodeBlock from its executable, and it runs once this script returns,
// so the resumptions above happen afterwards and must not reuse the entry points cached for them here.
$vm.deleteAllCodeWhenIdle();
