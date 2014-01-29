function bar(a,b,c,d,e,f,g,h,i,j,k) {
}

noInline(bar);

for (var i = 0; i < 10000000; ++i)
    bar();

