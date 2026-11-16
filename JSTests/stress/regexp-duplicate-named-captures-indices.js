//@ runDefault("--useRegExpJIT=0")

const m = /(?<x>a){2}z|(?<x>b){2}y|c/d.exec("aac");
if (!("x" in m.indices.groups))
    throw "Expected \"x\" in m.indices.groups";
