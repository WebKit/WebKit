noInline(Object.keys);

for (var i = 0; i < 2e5; ++i)
    Object.keys(new Error());
