//@ $skipModes << :lockdown if $architecture == "mips"

function bench(string, func)
{
    for (var i = 0; i < 1000; ++i)
        func();
}
noInline(bench);

function forRepeatCount(count, pos, utf16) {
    var base = "lalalalala".repeat(count);
    if (utf16) {
        base += "ϧ"; // arbitrary utf-16
    }

    var input = base;
    var label;
    const charToFind = !utf16 ? "z" : String.fromCodePoint(0x0245);
    switch (pos) {
        case -1: {
            input = charToFind + base;
            label = `beg ${utf16 ? "UChar" : "LChar"}`;
            break;
        }
        case 0: {
            label = `mid ${utf16 ? "UChar" : "LChar"}`;
            input =
            base.substring(0, (base.length / 2) | 0) +
            charToFind +
            base.substring((base.length / 2) | 0);
            break;
        }
        case 1: {
            label = `end ${utf16 ? "UChar" : "LChar"}`;
            input = base + charToFind;
            break;
        }
            // not found
        case 2: {
            label = `404 ${utf16 ? "UChar" : "LChar"}`;
            break;
        }
    }

    // force it to not be a rope
    input = input.split("").join("");

    function target() {
        input.indexOf(charToFind)
    }
    noInline(target);
    return bench(
        `<${label}> [${new Intl.NumberFormat()
                .format(input.length)
                .padStart("10,000,001".length)} chars] indexOf`,
        target
    );
}

function all(utf16) {
    forRepeatCount(100000, 2, !!utf16);
}
all(true);
