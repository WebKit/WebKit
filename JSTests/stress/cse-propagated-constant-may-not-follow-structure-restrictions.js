let notAGetterSetter = {whatever: 42};

function v2(v5) {
    const v10 = Object();
    if (v5) {
        const v12 = {set:Array};
        const v14 = Object.defineProperty(v10,"length",v12);
        const v15 = (140899729)[140899729];
    } else {
        v10.length = notAGetterSetter;
    }
    const v18 = new Uint8ClampedArray(49415);
    v18[1] = v10;
    const v19 = v10.length;
    let v20 = 0;
    while (v20 < 100000) {
        v20++;
    }
}
const v26 = v2();
for (let v32 = 0; v32 < 1000; v32++) {
    const v33 = v2(true);
}
