const value = {};
for (let i = 0; i < 100; ++i)
    value[i] = {};
for (let i = 0; i < 1e5 / 4; ++i)
    JSON.stringify(value);
