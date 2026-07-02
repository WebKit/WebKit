// This test ensures we properly remove unary expression stack entries when backtracing in the parser.

try {
    eval("delete((a=++(b=b)) => {})");
} catch { }

try {
    eval("!(({a=(++((t)=((e))))})=>{})");
} catch { }

