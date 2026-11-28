function* gen(count) {
    let v0 = 0, v1 = 1, v2 = 2, v3 = 3, v4 = 4, v5 = 5, v6 = 6, v7 = 7, v8 = 8, v9 = 9;
    let v10 = 10, v11 = 11, v12 = 12, v13 = 13, v14 = 14, v15 = 15, v16 = 16, v17 = 17, v18 = 18, v19 = 19;
    let v20 = 20, v21 = 21, v22 = 22, v23 = 23, v24 = 24, v25 = 25, v26 = 26, v27 = 27, v28 = 28, v29 = 29;
    let v30 = 30, v31 = 31, v32 = 32, v33 = 33, v34 = 34;
    for (let i = 0; i < count; ++i) {
        yield i;
        v0 += 1; v1 += 1; v2 += 1; v3 += 1; v4 += 1; v5 += 1; v6 += 1; v7 += 1; v8 += 1; v9 += 1;
        v10 += 1; v11 += 1; v12 += 1; v13 += 1; v14 += 1; v15 += 1; v16 += 1; v17 += 1; v18 += 1; v19 += 1;
        v20 += 1; v21 += 1; v22 += 1; v23 += 1; v24 += 1; v25 += 1; v26 += 1; v27 += 1; v28 += 1; v29 += 1;
        v30 += 1; v31 += 1; v32 += 1; v33 += 1; v34 += 1;
    }
    return v0 + v1 + v2 + v3 + v4 + v5 + v6 + v7 + v8 + v9 + v10 + v11 + v12 + v13 + v14 + v15 + v16 + v17 + v18 + v19 + v20 + v21 + v22 + v23 + v24 + v25 + v26 + v27 + v28 + v29 + v30 + v31 + v32 + v33 + v34;
}

let result = 0;
for (let i = 0; i < 1e4; ++i) {
    let iterator = gen(20);
    for (let value = iterator.next(); !value.done; value = iterator.next())
        result += value.value;
}
if (result !== 1e4 * 190)
    throw new Error(`bad result: ${result}`);
