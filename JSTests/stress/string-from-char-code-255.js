for (var i = 0; i <= testLoopCount; ++i) {
    if (String.fromCharCode(0xff) != '\u00ff')
        throw new Error("out");
}
