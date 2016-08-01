//@ runNoFTL

let success = false;
try {
    eval(`or ([[{break //comment
         [[{aFY sga=
         [[{a=Yth FunctionRY&=Ylet 'a'}V a`)
} catch(e) {
    success = e.toString() === "SyntaxError: Unexpected token '//'. Expected a ':' following the property name 'break'.";
}

if (!success)
    throw new Error("Bad result")
