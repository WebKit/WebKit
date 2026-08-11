//@ requireOptions("--useTemporal=1")
const plainTime = new Temporal.PlainTime(10, 30, 0);
for (var i = 0; i < 1e3; ++i)
    new Intl.DateTimeFormat("en-US").format(plainTime);
