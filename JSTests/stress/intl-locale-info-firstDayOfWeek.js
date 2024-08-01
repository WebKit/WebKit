function sameValue(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function shouldThrow(callback, errorConstructor) {
    try {
        callback();
    } catch (e) {
        if (!(e instanceof errorConstructor))
            throw new Error(`Expected ${errorConstructor.prototype.name} but got ${e.name}`);
        return;
    }
    throw new Error(`Expected ${errorConstructor.prototype.name} but got no error`);
}

{
    const loc = new Intl.Locale("en");
    sameValue(loc.firstDayOfWeek, undefined);
}

{
    const loc = new Intl.Locale("en", { firstDayOfWeek: "mon" });
    sameValue(loc.firstDayOfWeek, "mon");
}

{
    const weekdaysStrings = [
        ["0", "sun"],
        ["1", "mon"],
        ["2", "tue"],
        ["3", "wed"],
        ["4", "thu"],
        ["5", "fri"],
        ["6", "sat"],
        ["7", "sun"],
    ];
    for (const [weekday, string] of weekdaysStrings) {
        const loc = new Intl.Locale("en", { firstDayOfWeek: weekday });
        sameValue(loc.firstDayOfWeek, string);
    }
}

{
    const invalidFirstDayOfWeekOptions = ["", "m", "mo", "longerThan8Chars"];
    for (const firstDayOfWeek of invalidFirstDayOfWeekOptions) {
        shouldThrow(() => {
            new Intl.Locale("en", { firstDayOfWeek });
        }, RangeError);
    }
}
