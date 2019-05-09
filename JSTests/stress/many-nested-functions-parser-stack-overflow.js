var code = "function f1() {\n".repeat(80000); 
 
code += code; 
 
code += ", x" + -2147483648 + " = " + 1; 
 
code += ";\n"; 
 
code += "  return 80000;\n"; 
 
code += "}\n"; 
 
eval(code); 