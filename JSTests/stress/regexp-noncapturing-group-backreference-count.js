// Non-capturing groups should not increase the subpattern count.

function expectInvalid(pattern) {
    for (const mods of ["u", "v"]) {
        try {
            new RegExp(pattern, mods);
        } catch (e) {
            if (e.message.includes("invalid backreference"))
                continue;
            throw e;
        }

        throw "Expected an exception for /" + pattern + "/" + mods;
    }
}

[
    "(?:)\\1",
    "(?i:)\\1",
    "(?m:)\\1",
    "(?s:)\\1",
    "(?-i:)\\1",
    "(?-m:)\\1",
    "(?-s:)\\1",
    "(?i-m:)\\1",
    "(?m-s:)\\1",
].forEach(expectInvalid);
