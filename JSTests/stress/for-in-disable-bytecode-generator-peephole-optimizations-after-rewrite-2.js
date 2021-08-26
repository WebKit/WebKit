function foo() {
    for (var c in b) {
        if (b.hasOwnProperty(c)) {
            var e = b[c];
            c = "str";
        }
    }
}

try {
    foo();
} catch { }
