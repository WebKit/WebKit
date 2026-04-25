for (let i = 0; i < 10000; ++i) {
    const v23 = (2 ** 31) - 1;
    const v28 = (-1).toLocaleString(2, 31).padEnd(v23, "aa");
    try {
        eval(v28);
    } catch { }
}
