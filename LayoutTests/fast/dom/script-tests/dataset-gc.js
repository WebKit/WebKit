description("This tests that custom properties on element.dataset persist GC.");

function gc()
{
    if (window.GCController)
        return GCController.collect();

    for (var i = 0; i < 10000; i++) {
        var s = new String("");
    }
}


var d = document.createElement("div");

// Ensure the dataset is created.
var dataset = d.dataset;

// Ensure there is a __proto__ before test.
shouldBeNonNull("d.dataset.__proto__");

// Set __proto__ to non-starting value. (Must be null do to __proto__ restrictions).
d.dataset.__proto__ = null;
shouldBeNull("d.dataset.__proto__");

// Null out reference to the dataset. 
dataset = null;

gc();

// Test that the null persisted the GC.
shouldBeNull("d.dataset.__proto__");

var successfullyParsed = true;
