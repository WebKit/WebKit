let o = {};
for (let i = 0; i < 64; i++)
    o[i] = i;
for (let i = 0; i < 1e4; i++)
    Object.entries(o);
