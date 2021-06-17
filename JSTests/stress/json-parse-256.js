for (var j = 0; j < 3; ++j) {
    var text = `[`;
    for (var k = 0; k < 2; ++k) {
        for (var i = 0; i < 256; ++i)
            text += `"\\u00${i.toString(16).padStart(2, '0')}test",`;
    }
    text += `0 ]`;
    JSON.parse(text);
}

for (var j = 0; j < 3; ++j) {
    var text = `[`;
    for (var k = 0; k < 2; ++k) {
        for (var i = 0; i < 256; ++i)
            text += `"\\u00${i.toString(16).padStart(2, '0')}test",`;
    }
    text += `"ココア" ]`;
    JSON.parse(text);
}
