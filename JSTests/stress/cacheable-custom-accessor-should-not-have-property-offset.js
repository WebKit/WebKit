function main() {
    const v1 = {length:parseInt};
    for (let v6 = 0; v6 < 100; v6 = v6 + 2.0) {
        function v8(v9,v10,v11,v12) {
            try {
                const v13 = v9();
                const v15 = {set:parseInt};
                const v17 = Object.defineProperty(v13,4294967295,v15);
                v1.__proto__ = v13;
                const v18 = v1.arguments;
            } catch(v19) {
            }
            return v8;
        }
        const v21 = [293729.1679360643, 2635518607, 293729.1679360643, 293729.1679360643, 293729.1679360643];
        const v22 = v21.reduce(v8);
    }
}
noDFG(main);
noFTL(main);
main();
