function opt(f, length = 5) {
    (function () {
        f;
        length;
    })();

    return f();
}

function main() {
    opt(function () {});

    for (let i = 0; i < 10000; i++) {
        let threw = false;
        try {
            const iterator = opt(Array.prototype.keys);
            print([...iterator]);
        } catch {
            threw = true;
        }

        if (!threw)
            throw new Error();
    }
}

main();
