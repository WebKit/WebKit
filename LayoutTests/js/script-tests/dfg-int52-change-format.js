function foo(a) {
    // Create an int52.
    var x = a + 3000000000;
    // Make sure it's left-shifted.
    var y = x + x;
    // Now get it to be right-shifted.
    var z = x >> 1;
    // And finally, do something that prefers left-shift.
    var w = y + x;
    return [x, y, z, w];
}

dfgShouldBe(foo, "foo(1000000000)", "[4000000000,8000000000,-147483648,12000000000]");

