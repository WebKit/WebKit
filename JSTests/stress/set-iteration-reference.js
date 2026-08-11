function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function main() {
    let emptyValue = null;

    try {
        const set = new Set([1, 2, 3, 4, 5]);

        let cnt = 0;
        set.intersection({
            size: 10,
            has: key => {
                cnt++;

                if (cnt === 1) {
                    set.delete(1);
                    set.delete(2);
                    set.delete(3);
                    set.delete(4);
                } else {
                    emptyValue = key;
                    throw 1;
                }

                return false;
            },

            keys() {

            }
        });
    } catch {

    }

    shouldBe(emptyValue, 5);
}

main();
