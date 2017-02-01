var args = "y,".repeat(30000);
var g = Function(args, "return 0");
for (var i = 0; i < 1e3; ++i) {
    g();
}
