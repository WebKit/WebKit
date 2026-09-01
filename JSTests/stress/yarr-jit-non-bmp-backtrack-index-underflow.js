const re = /(?!\u{10000})a*|\u{10000}b/u;
const subj = "\u{10000}cc";
const m = re.exec(subj);
if (m[0].length > subj.length)
    throw new Error("m[0].length (" + m[0].length + ") > subj.length (" + subj.length + ")");
