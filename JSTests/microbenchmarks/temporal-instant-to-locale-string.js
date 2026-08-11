//@ requireOptions("--useTemporal=1")
const instant = new Temporal.Instant(1705314645000000000n);
for (var i = 0; i < 1e3; ++i)
    instant.toLocaleString();
