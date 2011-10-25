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
// Add a property to our prototype. It will be hidden by the corresponding data- attribute.
dataset.__proto__.customProperty = 1;
dataset.customProperty = 1; // Now set a property on ourselves.
shouldBe("d.getAttribute('data-custom-property')", "'1'");
shouldBe("d.dataset.customProperty", "'1'");

dataset = null;

gc();

// Test that the custom property persisted the GC.
shouldBe("d.dataset.customProperty", "'1'");
