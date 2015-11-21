description("Regression test for https://webkit.org/b/151495. - This test should not crash.");

var x = { a: 1, b: 2, c: 3, d: 4, e: 5, f: 6 };
for (i = 0; i < 2000; ++i) {
    // Keep adding new properties...
    x["foo" + i] = 1;
    // ...to force creation of new JSPropertyNameEnumerator objects.
    for (j in x) { }
}
