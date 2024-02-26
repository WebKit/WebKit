function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
  
let latest;
function foo() {
    for (let i = 0; i < 1e6; i++) {
        latest = this.isPureNaN(i);
    }
}
foo();
shouldBe(latest, false);
