var code = "function f1() {\n".repeat(80000); 
 
code += code; 
 
code += ", x" + -2147483648 + " = " + 1; 
 
code += ";\n"; 
 
code += "  return 80000;\n"; 
 
code += "}\n"; 

try {
    eval(code);
} catch (e) {
    if (!(e instanceof RangeError))
        throw new Error(`threw an error: ${e} but it wasn't a RangeError`);
}
