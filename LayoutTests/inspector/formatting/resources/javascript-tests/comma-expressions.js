a(x, y, z), b(x, y, z), c(x, y, z);

if (a(x, y, z), b(x, y, z), c(x, y, z)) { }

true && (a(x, y, z), b(x, y, z), c(x, y, z)) && true;

(x, y, z) => { a(x, y, z), b(x, y, z), c(x, y, z) }

(x, y, z) => { if (a(x, y, z), b(x, y, z), c(x, y, z)) { } }

(x, y, z) => { true && (a(x, y, z), b(x, y, z), c(x, y, z)) && true; }

(x, y, z) => (a(x, y, z), b(x, y, z), c(x, y, z));

(x, y, z) => true && (a(x, y, z), b(x, y, z), c(x, y, z)) && true;

function foo(x, y, z) { a(x, y, z), b(x, y, z), c(x, y, z) }

function foo(x, y, z) { if (a(x, y, z), b(x, y, z), c(x, y, z)) { } }

function foo(x, y, z) { true && (a(x, y, z), b(x, y, z), c(x, y, z)) && true; }
