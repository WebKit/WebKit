const value = { wide: "\u{1F600}" };
for (let i = 0; i < 200; ++i)
    value["abcdefgh" + i] = "ijklmnop" + i;
for (let i = 0; i < 5e4; ++i)
    JSON.stringify(value);
