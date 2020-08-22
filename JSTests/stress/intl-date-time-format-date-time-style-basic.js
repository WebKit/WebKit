function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

let now = 1592836312081;
{
    let o = new Intl.DateTimeFormat("en" , {
        timeStyle: "short",
        timeZone: "UTC",
    });
    shouldBe(o.format(now), `2:31 PM`);
}

{
    let o = new Intl.DateTimeFormat("en" , {
        dateStyle: "short",
        timeZone: "UTC",
    });
    shouldBe(o.format(now), `6/22/20`);
}

{
    let o = new Intl.DateTimeFormat("en" , {
        timeStyle: "medium",
        dateStyle: "short",
        timeZone: "UTC",
    });
    shouldBe(o.format(now), `6/22/20, 2:31:52 PM`);
}
