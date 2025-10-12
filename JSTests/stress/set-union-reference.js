function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function main() {
    let emptyValue = null;

    try {
        const set = new Set([1, 2, 3, 4, 5]);

        let cnt = 0;
        set.union({
            size: 10,
            has: key => {
                return true; // Not used in union, but required
            },

            keys() {
                return {
                    next() {
                        cnt++;

                        if (cnt === 1) {
                            set.delete(1);
                            set.delete(2);
                            set.delete(3);
                            set.delete(4);
                            return { value: 6, done: false };
                        } else {
                            emptyValue = 6; // The value we should have processed
                            throw 1;
                        }
                    }
                };
            }
        });
    } catch {
        // Expected to throw
    }

    shouldBe(emptyValue, 6);
}

main();
