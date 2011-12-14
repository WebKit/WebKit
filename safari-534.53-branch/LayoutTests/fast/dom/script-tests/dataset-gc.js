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

var dataset = d.dataset;
dataset.__proto__.customProperty = 1; // Add a property to our prototype, so it won't forward to our element.
dataset.customProperty = 1; // Now set a property on ourselves.
shouldBe("d.getAttribute('data-custom-property')", "null");
shouldBe("d.dataset.customProperty", "1");

dataset = null;

gc();

// Test that the custom property persisted the GC.
shouldBe("d.dataset.customProperty", "1");

var successfullyParsed = true;
