for (var i = 0; i <= 1e6; ++i) {
    if (String.fromCharCode(0xff) != '\u00ff')
        throw new Error("out");
}
