description('This tests that properties on the classList persists GC.');

function gc()
{
    if (window.GCController)
        return GCController.collect();

    for (var i = 0; i < 10000; i++) {
        var s = new String;
    }
}

var d = document.createElement('div');

// Ensure the classList is created.
var classList = d.classList;

// Set a custom property.
d.classList.life = 42;
shouldEvaluateTo('d.classList.life', 42);

// Null out reference to the dataset.
classList = null;

gc();

// Test that the classList wrapper persisted the GC and still has the custom property.
shouldEvaluateTo('d.classList.life', 42);

var successfullyParsed = true;
