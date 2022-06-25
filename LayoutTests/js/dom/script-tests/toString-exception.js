description(
'This test checks for regression against <a href="https://bugs.webkit.org/show_bug.cgi?id=7343">7343: REGRESSION: js/toString-overrides.html fails when run multiple times</a>.'
);

var a = [{ toString : 0 }];

try {
    a.toString();
} catch (e) {
}

var caught = false;

try {
  a.toString();
} catch (e) {
caught = true;
}

shouldBeTrue("caught");
