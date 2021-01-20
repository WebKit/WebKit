function main() {
    function v0() {
        let v4 = {};

        let v7 = 0;
        while (v7 < 1000) {
            v7++;
            v4.a = {};
        }

        v4.a = v4;

        let v19 = 0;
        while (v19 < 90) {
            with (parseInt) { }
            v19++;
        }
    }

    for (var i = 0; i < 1000; i++) {
        const v33 = v0();
    }
}
noDFG(main);
noFTL(main);
main();
