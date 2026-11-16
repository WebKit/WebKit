// Async functions without await returning an object literal should fulfill the
// result promise inline once constant folding proves the resolved value is not
// a thenable, instead of calling operationNewResolvedPromise every time.

const rows = [];
for (let j = 0; j < 16; j++)
    rows.push({ id: j, first: 'A' + j, last: 'B' + j, score: j * 3 });

async function toDTO(row) {
    return { id: row.id, name: row.first, score: row.score, ok: true };
}

function kernel(count) {
    let last;
    for (let i = 0; i < count; i++)
        last = toDTO(rows[i & 15]);
    return last;
}
noInline(kernel);

let last;
for (let r = 0; r < 5; r++)
    last = kernel(8e5);

last.then((dto) => {
    if (dto.score !== rows[(8e5 - 1) & 15].score)
        throw new Error("bad result");
});
drainMicrotasks();
