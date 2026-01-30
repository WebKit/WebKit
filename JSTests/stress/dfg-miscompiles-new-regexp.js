function opt(regExp) {
    return new RegExp(regExp, undefined);
}

function main() {
    const regExp = /a/;

    for (let i = 0; i < testLoopCount; i++) {
        if (opt(regExp) === regExp) {
            throw new Error(`Bug triggered at ${i}`);
            break;
        }
    }
}

main();
