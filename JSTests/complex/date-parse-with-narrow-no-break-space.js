function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
var space = ' ';
var narrowNoBreakSpace = '\u202f';

shouldBe(new Date(`11/16/2022, 10:33:58${space}AM`).getTime(), 1668623638000);
shouldBe(new Date(`11/16/2022, 10:33:58${narrowNoBreakSpace}AM`).getTime(), 1668623638000);
shouldBe(Date.parse(`11/16/2022, 10:33:58${space}AM`), 1668623638000);
shouldBe(Date.parse(`11/16/2022, 10:33:58${narrowNoBreakSpace}AM`), 1668623638000);
