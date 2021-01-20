function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
noInline(shouldBe);

function test()
{
    this.x = 'a'.match(/a/)['groups'];
    shouldBe(this.x, undefined);
}
noInline(test);
for (let i=0; i<100000; i++)
    test();
