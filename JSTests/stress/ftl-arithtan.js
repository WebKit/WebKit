function foo(x) {
    return Math.tan(x);
}

noInline(foo);

var j = 0;
for (var i = 0; i < 100000; ++i)
    j = foo(i);

if (-1.6871736258025631 != j){
    throw `Error: ${j}`;
}
