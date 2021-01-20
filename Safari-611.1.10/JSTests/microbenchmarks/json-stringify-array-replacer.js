const replacer = new Array(15).fill('key');
for (let i = 0; i < 1e6; ++i)
    JSON.stringify(null, replacer);
