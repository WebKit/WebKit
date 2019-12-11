description(
"Make sure we don't crash when compiling put_to_base in the baseline JIT."
);

var sum = 0;
function globalF() {
    return 42; 
}

// Create a watchpoint on globalF.
var warmup = function() {
    sum += globalF();
}

for (var i = 0; i < 100; i++) {
    warmup();
}

var foo = function(o) {
    if (o.x > 10) 
        eval("globalF = function() { return 43; }");
    else
        sum += globalF();
};

var o = {}; 

// Tier up to JIT for crashy crash.
for (var i = 0; i < 100; i++) {
    o.x = i;
    foo(o);
}

shouldBe("sum", "4662"); 
