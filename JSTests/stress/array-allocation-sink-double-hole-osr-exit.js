
// Even iterations leave arr[1] as a hole. The loop needs enough iterations for
// FTL allocation sinking to kick in; the break fires on an even iteration so
// arr[1] is unwritten (a hole).
let ftlV, ftlHas;
let breakAt = testLoopCount - 2;
if (breakAt % 2 !== 0) breakAt--;
for (let i = 0; i < testLoopCount; i++) {
    let arr = new Array(4);
    arr[0] = 1.1;
    arr[2] = 3.3;
    if (i & 1) arr[1] = 2.2;
    arr[0] = i + 0.5;
    if (i === breakAt) { ftlV = arr[1]; ftlHas = 1 in arr; break; }
}
if (ftlV !== undefined)
    throw new Error("expected undefined, got " + ftlV);
if (ftlHas !== false)
    throw new Error("expected false, got " + ftlHas);
