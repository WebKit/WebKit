// @requireOptions("--slowPathAllocsBetweenGCs=10")

function getRope(index) {
    const a = "[" + index + ',[]'.repeat(0x100) + "]";
    const b = "()";

    [][a];

    return a + b;
}

function main() {
    for (let i = 0; i < 1000; i++) {
        const s = getRope(i);
        getRope(0);

        gc();

        try {
            eval(s);

        } catch {

        }
    }

}

main();
