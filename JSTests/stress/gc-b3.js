function main() {
    noDFG(main);

    const object = {
        opt: new Function('value', `
            function inlinee(value) {
                return [-value, -value, -value];
            }

            return inlinee(value);`)
    };

    for (let i = 0; i < 10000; i++) {
        object.opt(BigInt(i));
    }

    setTimeout(() => {
        object.opt = null;
        gc();

        setTimeout(() => {
            print('Should\'ve crashed.');
        }, 1000);
    }, 300);
}

main();
