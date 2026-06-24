//@ requireOptions("--useTemporal=1")
const plainDate = new Temporal.PlainDate(2024, 1, 15);
for (var i = 0; i < 1e3; ++i)
    plainDate.toLocaleString();
