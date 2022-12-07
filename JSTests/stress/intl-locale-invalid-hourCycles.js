function touch(arg) {
    return arg + "Hello"; // Touching.
}
noInline(touch);

function main() {
    const v24 = new Intl.Locale("trimEnd", { 'numberingSystem': "foobar" });
    let empty = v24.hourCycles();
    touch(empty);
}

try {
    main();
} catch (e) {
    if (!(e instanceof TypeError))
        throw e;
}
