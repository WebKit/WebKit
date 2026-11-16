//@ requireOptions("--useTemporal=1")
const plainDateTime = new Temporal.PlainDateTime(2024, 1, 15, 10, 30, 45);
for (var i = 0; i < 1e3; ++i)
    plainDateTime.toLocaleString();
