function t(n) {
    switch (n) {
        case 1:
            function f() {
                return 10;
            }
            break;
        case 2:
            function f() {
                return 20;
            }
            break;
    }

    try {
      return f();
    } catch (e) {
      return -1;
    }
}

shouldBe(t(1), '20');
shouldBe(t(2), '20');
shouldBe(t(3), '20');

var successfullyParsed = true;
