function forceArrayMaterialization(idx) {
    let a = new Array(3);
    a[0] = idx;
    a[1] = idx + 1;
    a[2] = idx + 2;

    let butterfly = a.length;

    if (idx % 7 === 0) {
        return a;
    }

    return butterfly * 2;
}
noInline(forceArrayMaterialization);

for (let i = 0; i < testLoopCount; i++) {
    forceArrayMaterialization(i);
}