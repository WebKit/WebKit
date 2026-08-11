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
    shouldBe(JSON.stringify(o.resolvedOptions()), `{"locale":"en","calendar":"gregory","numberingSystem":"latn","timeZone":"UTC","hourCycle":"h12","hour12":true,"timeStyle":"short"}`);
}

{
    let o = new Intl.DateTimeFormat("en" , {
        dateStyle: "short",
        timeZone: "UTC",
    });
    shouldBe(o.format(now), `6/22/20`);
    shouldBe(JSON.stringify(o.resolvedOptions()), `{"locale":"en","calendar":"gregory","numberingSystem":"latn","timeZone":"UTC","dateStyle":"short"}`);
}

{
    let o = new Intl.DateTimeFormat("en" , {
        timeStyle: "medium",
        dateStyle: "short",
        timeZone: "UTC",
    });
    shouldBe(o.format(now), `6/22/20, 2:31:52 PM`);
    shouldBe(JSON.stringify(o.resolvedOptions()), `{"locale":"en","calendar":"gregory","numberingSystem":"latn","timeZone":"UTC","hourCycle":"h12","hour12":true,"dateStyle":"short","timeStyle":"medium"}`);
}
