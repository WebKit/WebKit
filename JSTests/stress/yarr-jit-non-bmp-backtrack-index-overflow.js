const re = /d\0e?|\u{10000}c/u;
const subj = "\u{10000}d";
const m = re.exec(subj);

if (m !== null) {
    throw new Error(
        "expected null, got match=" + JSON.stringify(m[0]) +
        " at index=" + m.index +
        " (m.index + m[0].length = " + (m.index + m[0].length) +
        " > subj.length = " + subj.length + ")"
    );
}
