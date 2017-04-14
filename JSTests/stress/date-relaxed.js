function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error(`bad value: ${actual}`);
}

{
    let date = new Date("May 8");
    shouldBe(date.getFullYear(), 2000);
    shouldBe(date.getMonth(), 4);
    shouldBe(date.getDate(), 8);
}
{
    let date = new Date("Feb 29");
    shouldBe(date.getFullYear(), 2000);
    shouldBe(date.getMonth(), 1);
    shouldBe(date.getDate(), 29);
}
{
    let date = new Date(" May 8 ");
    shouldBe(date.getFullYear(), 2000);
    shouldBe(date.getMonth(), 4);
    shouldBe(date.getDate(), 8);
}
{
    let date = new Date(" Feb 29 ");
    shouldBe(date.getFullYear(), 2000);
    shouldBe(date.getMonth(), 1);
    shouldBe(date.getDate(), 29);
}
{
    let date = new Date("May/8");
    shouldBe(date.getFullYear(), 2000);
    shouldBe(date.getMonth(), 4);
    shouldBe(date.getDate(), 8);
}
{
    let date = new Date("Feb/29");
    shouldBe(date.getFullYear(), 2000);
    shouldBe(date.getMonth(), 1);
    shouldBe(date.getDate(), 29);
}
{
    let date = new Date("May8");
    shouldBe(date.getFullYear(), 2000);
    shouldBe(date.getMonth(), 4);
    shouldBe(date.getDate(), 8);
}
{
    let date = new Date("Feb29");
    shouldBe(date.getFullYear(), 2000);
    shouldBe(date.getMonth(), 1);
    shouldBe(date.getDate(), 29);
}

{
    let date = new Date("May 8 -1");
    shouldBe(date.getFullYear(), -1);
    shouldBe(date.getMonth(), 4);
    shouldBe(date.getDate(), 8);
}
