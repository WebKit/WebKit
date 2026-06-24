//@ requireOptions("--useTemporal=1", "--validateExceptionChecks=1")

function opt(a1) {
    var a2 = new Temporal.PlainYearMonth(((NaN * a1 <= 0).__proto__ = "\\u{1F4A9}ba"), 1, 'chinese', 1);
    a3 = a1;
    let a4 = a4.withPlainTime(a3);
    throw a1;
}
try { opt((function* () { }.constructor)) } catch (y) { }
try { opt(9007199254740991) } catch (y) { }
try { opt("\\u{10428}\\u{10000}x") } catch (y) { }
