function sameArray(a, b) {
    if (a.length !== b.length)
        throw new Error(`length: expected ${b.length} but got ${a.length}`);
    for (let i = 0; i < a.length; ++i) {
        if (a[i] !== b[i])
            throw new Error(`${i}: expected ${b[i]} but got ${a[i]}`);
    }
}

/* int32 → int32, delete only */
{
    const src = [0, 1, 2, 3, 4, 5];
    sameArray(src.toSpliced(2, 2), [0, 1, 4, 5]);
    sameArray(src, [0, 1, 2, 3, 4, 5]);
}

/* int32 → int32, insert int32 */
{
    const src = [10, 11, 12, 13];
    sameArray(src.toSpliced(2, 0, 99, 98), [10, 11, 99, 98, 12, 13]);
    sameArray(src, [10, 11, 12, 13]);
}

/* int32 → double, insert double */
{
    const src = [1, 2, 3, 4];
    sameArray(src.toSpliced(2, 1, 3.25, 4.25), [1, 2, 3.25, 4.25, 4]);
    sameArray(src, [1, 2, 3, 4]);
}

/* int32 → contiguous, insert object */
{
    const src = [7, 8, 9];
    const obj = { v: 42 };
    sameArray(src.toSpliced(1, 1, obj), [7, obj, 9]);
    sameArray(src, [7, 8, 9]);
}

/* double → double, delete last */
{
    const src = [0.5, 1.5, 2.5, 3.5];
    sameArray(src.toSpliced(-1), [0.5, 1.5, 2.5]);
    sameArray(src, [0.5, 1.5, 2.5, 3.5]);
}

/* double → double, insert int32 */
{
    const src = [5.5, 6.5, 7.5];
    sameArray(src.toSpliced(1, 0, 100), [5.5, 100, 6.5, 7.5]);
    sameArray(src, [5.5, 6.5, 7.5]);
}

/* double → contiguous, insert string & object */
{
    const src = [2.2, 3.3, 4.4];
    const o = { x: 1 };
    sameArray(src.toSpliced(0, 1, "str", o), ["str", o, 3.3, 4.4]);
    sameArray(src, [2.2, 3.3, 4.4]);
}

/* contiguous → contiguous, delete middle */
{
    const a = { a: 1 };
    const b = { b: 2 };
    const c = { c: 3 };
    const src = [a, b, c];
    sameArray(src.toSpliced(1, 1), [a, c]);
    sameArray(src, [a, b, c]);
}

/* contiguous → contiguous, insert mix */
{
    const src = ["x", "y", "z"];
    sameArray(src.toSpliced(2, 0, 7, 8.8), ["x", "y", 7, 8.8, "z"]);
    sameArray(src, ["x", "y", "z"]);
}

