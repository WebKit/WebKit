//@ runDefault

// Regression test for bug 180761.  This test should not crash.

if (/(?:(?: |a)|\.a)* a*/.exec("/a.aaa") !== null)
    throw "Expected /(?:(?: |a)|\.a)* a*/.exec(\"/a.aaa\") to not match";
