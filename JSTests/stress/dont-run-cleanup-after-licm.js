function foo(string) {
    var result1, result2;
    for (var i = 0; i < 1000; ++i) {
        result1 = string[0]; 
        for (var j = 0; j < 10; ++j)
            result2 = 1;
    }
   return [result1, result2];
}
noInline(foo);

foo(" ");
for (var i = 0; i < 1000; i++)
    foo(new Error());
