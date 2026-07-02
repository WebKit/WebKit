function main() {
    const v24 = new Intl.Locale("trimEnd", { 'numberingSystem': "foobar" });
    let empty = v24.getHourCycles();
    print(empty);
}

try {
    main();
} catch (e) {
    if (!(e instanceof TypeError))
        throw e;
}
