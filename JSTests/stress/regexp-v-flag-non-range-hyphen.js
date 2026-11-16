function compareArray(a, b) {
    if (a.length !== b.length)
        throw new Error(`Expected length ${b.length} but got length ${a.length}`);
    for (let i = 0; i < a.length; i++) {
        if (a[i] !== b[i])
            throw new Error(`${i}: Expected ${b[i]} but got ${a[i]}`);
    }
}

function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

{
    const regex = /[\d\-a]/v;

    for (let i = 0; i < 10; i++)
        compareArray(regex.exec(i.toString()), [i.toString()]);
    compareArray(regex.exec("-"), ["-"]);
    compareArray(regex.exec("a"), ["a"]);

    shouldBe(regex.exec("b"), null);
}

{
    const regex = /[\p{sc=Hiragana}\-a]/v;

    compareArray(regex.exec("あ"), ["あ"]);
    compareArray(regex.exec("い"), ["い"]);
    compareArray(regex.exec("ん"), ["ん"]);
    compareArray(regex.exec("-"), ["-"]);
    compareArray(regex.exec("a"), ["a"]);

    shouldBe(regex.exec("b"), null);
}

{
    const regex = /[\d\x2Da]/v;

    for (let i = 0; i < 10; i++)
        compareArray(regex.exec(i.toString()), [i.toString()]);
    compareArray(regex.exec("-"), ["-"]);
    compareArray(regex.exec("a"), ["a"]);

    shouldBe(regex.exec("b"), null);
}
