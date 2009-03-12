description("A couple of tests to ensure that adding a stop with an invalid color to a gradient throws exceptions normally, but not in dashboard.");
debug("Ensure exceptions are thrown in normal pages");
var ctx = document.createElement('canvas').getContext('2d');
var gradient = ctx.createLinearGradient(0, 0, 0, 100);
shouldThrow("gradient.addColorStop(1, 'rgb(NaN%, NaN%, NaN%)')");
var gradient = ctx.createRadialGradient(0, 0, 0, 100, 0, 0);
shouldThrow("gradient.addColorStop(1, 'rgb(NaN%, NaN%, NaN%)')");

if (this.layoutTestController) {
    debug("Switching to dashboard compatibility mode.  Invalid color strings should no longer cause an exception.");
    layoutTestController.setUseDashboardCompatibilityMode(true);
} else {
    debug("The following tests will fail in the browser as we can only enable dashboard compatibility mode in DRT.")
}

var gradient = ctx.createLinearGradient(0, 0, 0, 100);
shouldBeUndefined("gradient.addColorStop(1, 'rgb(NaN%, NaN%, NaN%)')");
var gradient = ctx.createRadialGradient(0, 0, 0, 100, 0, 0);
shouldBeUndefined("gradient.addColorStop(1, 'rgb(NaN%, NaN%, NaN%)')");

var successfullyParsed = true;
