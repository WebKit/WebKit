function Ctor1() {
    this.case = 1;
    this.something = 2;
}
function Ctor2() {
    this.default = 1;
    this.something = 2;
}

switch (x) {
case "abc":
    return this.default;
case "def":
    return this.case;
default:
    return 3;
}

switch (x) {
case "abc":
    return 1;
case "def":
    return 2;
default:
    return 3;
}

