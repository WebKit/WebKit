try {
    let char16 = decodeURI('%E7%9A%84');
    let rope = char16.padEnd(2147483644, 1);
    rope.__defineGetter__(256, function () {});
} catch { }
