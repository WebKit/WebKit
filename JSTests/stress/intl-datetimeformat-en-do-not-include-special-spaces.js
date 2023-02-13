function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(result) {
    shouldBe(result.includes('\u202f'), false);
    shouldBe(result.includes('\u2009'), false);
}

let date1 = new Date("2023-02-13T05:18:08.347Z");
let date2 = new Date("2023-02-23T05:18:08.347Z");
{
    let result = date1.toLocaleString('en');
    test(result);
}
{
    let result = date1.toLocaleString('en-US');
    test(result);
}
{
    let fmt = new Intl.DateTimeFormat('en', { timeStyle: "long" });
    test(fmt.format(date1));
    test(fmt.formatRange(date1, date2));
    fmt.formatToParts(date1).forEach((part) => {
        test(part.value);
    });
    fmt.formatRangeToParts(date1, date2).forEach((part) => {
        test(part.value);
    });
}
{
    let fmt = new Intl.DateTimeFormat('en-US', { timeStyle: "long" });
    test(fmt.format(date1));
    test(fmt.formatRange(date1, date2));
    fmt.formatToParts(date1).forEach((part) => {
        test(part.value);
    });
    fmt.formatRangeToParts(date1, date2).forEach((part) => {
        test(part.value);
    });
}
