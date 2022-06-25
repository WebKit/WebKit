function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function test() {
    let c = 0;
    shouldBe(`${Object.entries({ [++c]: ++c })}`, '1,2');

    {
        let c = 0;
        shouldBe(`${Object.entries({ [++c]: ++c })}`, '1,2');
    }
}
noInline(test);

for (let i = 0; i < 10000; i++)
  test();
