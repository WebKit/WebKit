// Regression test for bug 177570

if (/(Q)+|(\S)+Z/.test("Z "))
    throw "/(Q)+|(\S)+Z/.test(\"Z \") should fail, but actually succeeds";
