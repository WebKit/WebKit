let v8 = 0;
function v0(v1,v2,v3,v4,v5) {
    const v7 = /a/;
    function v9() {
        const v10 = v8++;
    }
    const v11 = {"valueOf":v9};
    v7.lastIndex = v11;
    const v12 = v7.test("zzzz");
    return v8;
}

for (let v19 = 0; v19 < 10000; v19++) {
  v0();
}

if (v8 !== 10000) throw new Error("expected v8 to be incremented 10000 times");
