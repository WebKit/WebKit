function test1() {
    var p = 0;
    switch (p) {
    case (p = 1):
        throw new Error("should not be reached");
    case 0:
        break;
    }
}

function test2() {
    var p = 1;
    switch (p) {
    case 2:
        break;
    case (p = 3):
        throw new Error("should not be reached");
    }
}

function test3() {
    var p = 0;
    var result;

    switch (p) {
    case (p = 1):
        result = "wrong";
        break;
    case p:
        result = "wrong";
        break;
    case 0:
        result = "ok";
        break;
    }

    if (result !== "ok")
        throw new Error("Bad Result: " + result);
}

test1();
test2();
test3();
