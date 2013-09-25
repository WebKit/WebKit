function Foo() {
    this.bar = function() { return 1; };
}

var sum = 0;
for (var i = 0; i < 100000; i++) {
    var f = new Foo();
    sum += f.bar();
}

if (sum != 100000)
    throw "Error: incorrect sum. Expected 10000 but got " + sum + ".";
