function main() {
const v3 = [1337,1337,13.37,1337];
const v5 = [1337,13.37,1337,1337,1337,1337,13.37,1337,1337,1337];
const v8 = {getInt8:13.37};
const v9 = Object();
function v10(v11,v12,v13,v14) {
    for (const v15 of v5) {
        for (const v16 of v11) {
            let v18 = v8;
            do {
                const v20 = Object.is(0,v18);
                const v22 = ["name"];
                for (let v25 = 0; v25 < 100; v25++) {
                    const v26 = v25[100];
                }
                const v27 = v22 + 1;
                v18 = v27;
            } while (v18 < -9007199254740991);
        }
    }
}
const v28 = v10(v3,v9);
}
noDFG(main);
noFTL(main);
main();
