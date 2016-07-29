// Regression test for 160329. This test should not crash or throw an exception.

function narrow(x) {
    return x << 24 >> 24;
}

noInline(narrow);

for (var i = 0; i < 1000000; i++) {
    let expected = i << 24;
    let got = narrow(i);
    expected = expected >> 24;

    if (expected != got)
        throw "FAILURE, expected:" + expected + ", got:" + got;
}

