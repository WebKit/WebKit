//@ requireOptions("--useTemporal=1")
const plainDateTime = new Temporal.PlainDateTime(2024, 1, 15, 10, 30, 0);
for (var i = 0; i < 1e3; ++i)
    new Intl.DateTimeFormat("en-US").format(plainDateTime);
