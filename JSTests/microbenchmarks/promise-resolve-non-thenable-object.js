for (var i = 0; i < 1e6; ++i)
    Promise.resolve({ a: i, b: i + 1, c: i + 2 });
