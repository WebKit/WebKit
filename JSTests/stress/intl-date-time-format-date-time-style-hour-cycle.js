function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

let now1 = 1592870440000;
let now2 = 1592827240000;

{
    let o = new Intl.DateTimeFormat("en" , {
        timeStyle: "short",
        timeZone: "UTC",
    });
    shouldBe(o.format(now1), `12:00 AM`);
}

{
    let o = new Intl.DateTimeFormat("en" , {
        timeStyle: "short",
        timeZone: "UTC",
        hourCycle: "h23",
    });
    shouldBe(o.format(now1), `00:00`);
}

{
    let o = new Intl.DateTimeFormat("en" , {
        timeStyle: "short",
        timeZone: "UTC",
        hourCycle: "h24",
    });
    shouldBe(o.format(now1), `24:00`);
}

{
    let o = new Intl.DateTimeFormat("en" , {
        timeStyle: "short",
        timeZone: "UTC",
        hourCycle: "h11",
    });
    shouldBe(o.format(now1), `0:00 AM`);
}

{
    let o = new Intl.DateTimeFormat("en" , {
        timeStyle: "short",
        timeZone: "UTC",
        hourCycle: "h12",
    });
    shouldBe(o.format(now1), `12:00 AM`);
}

{
    let o = new Intl.DateTimeFormat("en" , {
        timeStyle: "short",
        timeZone: "UTC",
        hour12: true,
    });
    shouldBe(o.format(now1), `12:00 AM`);
}

{
    let o = new Intl.DateTimeFormat("en" , {
        timeStyle: "short",
        timeZone: "UTC",
        hour12: false,
    });
    shouldBe(o.format(now1), `00:00`);
}

{
    let o = new Intl.DateTimeFormat("en" , {
        timeStyle: "short",
        timeZone: "UTC",
    });
    shouldBe(o.format(now2), `12:00 PM`);
}

{
    let o = new Intl.DateTimeFormat("en" , {
        timeStyle: "short",
        timeZone: "UTC",
        hourCycle: "h23",
    });
    shouldBe(o.format(now2), `12:00`);
}

{
    let o = new Intl.DateTimeFormat("en" , {
        timeStyle: "short",
        timeZone: "UTC",
        hourCycle: "h24",
    });
    shouldBe(o.format(now2), `12:00`);
}

{
    let o = new Intl.DateTimeFormat("en" , {
        timeStyle: "short",
        timeZone: "UTC",
        hourCycle: "h11",
    });
    shouldBe(o.format(now2), `0:00 PM`);
}

{
    let o = new Intl.DateTimeFormat("en" , {
        timeStyle: "short",
        timeZone: "UTC",
        hourCycle: "h12",
    });
    shouldBe(o.format(now2), `12:00 PM`);
}

{
    let o = new Intl.DateTimeFormat("en" , {
        timeStyle: "short",
        timeZone: "UTC",
        hour12: true,
    });
    shouldBe(o.format(now2), `12:00 PM`);
}

{
    let o = new Intl.DateTimeFormat("en" , {
        timeStyle: "short",
        timeZone: "UTC",
        hour12: false,
    });
    shouldBe(o.format(now2), `12:00`);
}
