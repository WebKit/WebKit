function test(a, b, c) { }

for (var i = 0; i < 1e6; ++i)
    test.bind(this, 0).bind(this, 1);
