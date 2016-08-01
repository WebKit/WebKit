var splited = "a".split(/$/);
if (splited.length != 1 || splited[0] != "a") 
    throw "Error: " + splited.length + " = " + splited;
