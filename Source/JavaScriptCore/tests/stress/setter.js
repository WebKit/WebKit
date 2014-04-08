function foo(o, v1) {
    o.f = v1;
    o.k = v1 * 33;
}

noInline(foo);

for (var i = 0; i < 100; ++i) {
    var o = {g_: 5};
    o.__defineSetter__("f", function(value) { this.g_ += 42 * value; });
    o.__defineSetter__("g", function(value) { this.g_ += 43 * value; });
    o.__defineSetter__("h", function(value) { this.g_ += 44 * value; });
    o.__defineSetter__("i", function(value) { this.g_ += 45 * value; });
    o.__defineSetter__("j", function(value) { this.g_ += 46 * value; });
    o.__defineSetter__("k", function(value) { this.g_ += 47 * value; });
    foo(o, 29);
    if (o.g_ != 5 + 42 * 29 + 29 * 47 * 33)
        throw "Error: bad result: " + o.g_;
}

// Test the case where those fields aren't setters anymore.
var o = {g_: 5};
o.f = 1;
o.g = 2;
o.h = 3;
o.i = 4;
o.j = 5;
o.k = 6;
foo(o, 29);
if (o.g_ != 5)
    throw "Error: bad value of g_: " + o.g_;
if (o.f != 29)
    throw "Error: bad value of f: " + o.f;
if (o.k != 29 * 33)
    throw "Error: bad value of k: " + o.k;

// Test the case where they are setters but they're not the same setters.
var o = {g_: 5};
o.__defineSetter__("f", function(value) { this.g_ += 52 * value; });
o.__defineSetter__("g", function(value) { this.g_ += 53 * value; });
o.__defineSetter__("h", function(value) { this.g_ += 54 * value; });
o.__defineSetter__("i", function(value) { this.g_ += 55 * value; });
o.__defineSetter__("j", function(value) { this.g_ += 56 * value; });
o.__defineSetter__("k", function(value) { this.g_ += 57 * value; });
foo(o, 29);
if (o.g_ != 5 + 52 * 29 + 29 * 57 * 33)
    throw "Error: bad result at end: " + o.g_;
