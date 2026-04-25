description(
"This tests that modulo with zero does not crash but returns NaN."
);

function mod(b) {
    return b%b;
}

shouldBeNaN("mod(0)");
