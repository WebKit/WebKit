//@ runDefault("--useFTLJIT=0", "--slowPathAllocsBetweenGCs=100", "--forceDebuggerBytecodeGeneration=1", "--forceEagerCompilation=1")
for (let i1 = 0; i1 < 100; i1++) {
    const v8 = { a: 2, b: i1 };
    var obj = v8;
    for (let i20 = (() => {
            const v10 = +v8;
            v10 / v10;
            const v16 = new ArrayBuffer(64, { maxByteLength: 26843 });
            new Float32Array(v16);
            return 0;
        })();
        (() => {
            const v22 = i20 < 40;
            v8[40] = v22;
            const v23 = [];
            const v24 = {};
            v24.a = 42;
            v24.b = 42;
            v24.c = 42;
            v24.d = 42;
            v24.e = 42;
            v23.push(v24);
            const v31 = {};
            v31.e = 42;
            v31.d = 42;
            v31.c = 42;
            v31.b = 42;
            v31.a = 42;
            v23.push(v31);
            const v38 = {};
            v38.a = 42;
            v38.b = 42;
            v38.c = 42;
            v38.d = 42;
            v38.e = 42;
            v23.push(42);
            const v45 = {};
            v45.e = 42;
            v45.d = 42;
            v45.c = 42;
            v45.b = 42;
            v45.a = 42;
            v23.push(v45);
            const v52 = {};
            v52.a = 42;
            v52.b = 42;
            v52.c = 42;
            v52.d = 42;
            v52.e = 42;
            v23.push(v52);
            return v22;
        })();
        i20++) {
        const v21 = ("p" + i1) & "|";
        v21 + v21;
        var d = v8;
        obj[v21] = d;
        obj[1] = 2;
        const v69 = obj.b;
        try { d.return(v69, 2); } catch (e) {}
        obj[2] = i1;
        obj.p = 2;
    }
}
