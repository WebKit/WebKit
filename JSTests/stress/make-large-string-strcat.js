var s = "s";

try {
    for (var i = 0; i < 31; ++i)
        s = "t" + s + s;
    print("Should not have gotten here.");
    print("String length: " + s.length);
    throw "Should not have gotten here.";
} catch (e) {
    if (e.message != "Out of memory")
        throw "Wrong error: " + e;
}
