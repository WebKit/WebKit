//@ requireOptions("--useTemporal=1")
const plainTime = new Temporal.PlainTime(10, 30, 45);
for (var i = 0; i < 1e3; ++i)
    plainTime.toLocaleString();
