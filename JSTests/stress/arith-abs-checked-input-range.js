let enteredFirstBranch = 0;
let enteredBoundsBranch = 0;
let lastIndex = 0;

function probe(x)
{
    const target = new Uint8Array(new ArrayBuffer(32));

    let dead = x;
    dead++;
    dead++;

    const first = Math.abs(x + 2);
    if (first < 1) {
        enteredFirstBranch++;
        const index = first + 16;
        lastIndex = index;
        if (index < target.length) {
            enteredBoundsBranch++;
            return target[index];
        }
    }
    return 0;
}
noInline(probe);

for (let i = 0; i < 30000; ++i)
    probe(-2);

enteredFirstBranch = 0;
enteredBoundsBranch = 0;
lastIndex = 0;

const result = probe(2147483646);
if (result !== 0)
    throw new Error(`Bad result: ${result}`);
if (enteredFirstBranch)
    throw new Error(`Entered first branch: ${enteredFirstBranch}`);
if (enteredBoundsBranch)
    throw new Error(`Entered bounds branch: ${enteredBoundsBranch}`);
if (lastIndex)
    throw new Error(`Computed wrapped index: ${lastIndex}`);
