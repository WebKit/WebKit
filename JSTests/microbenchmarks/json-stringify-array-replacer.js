var replacer = new Array(25).fill('key');
for (var i = 0; i < 1e5; ++i)
    JSON.stringify(null, replacer);
