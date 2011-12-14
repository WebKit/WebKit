description("Tests that accessing Attr after its Element has been destroyed works without crashing.");

function gc()
{
    if (window.GCController)
        return GCController.collect();

    // Trigger garbage collection indirectly.
    for (var i = 0; i < 100000; i++)
        new String(i);
}

var element = document.createElement("p");
element.setAttribute("a", "b");
var attributes = element.attributes;
element = null;

gc();

shouldBe("attributes.length", "1");
shouldBe("attributes[0]", "attributes.item(0)");
shouldBe("attributes.getNamedItem('a')", "attributes.item(0)");

shouldBe("attributes.item(0).name", "'a'");
shouldBe("attributes.item(0).specified", "true");
shouldBe("attributes.item(0).value", "'b'");
shouldBe("attributes.item(0).ownerElement.tagName", "'P'");

attributes.item(0).value = 'c';

shouldBe("attributes.item(0).value", "'c'");

attributes.removeNamedItem('a');

shouldBe("attributes.length", "0");

element = document.createElement("p");
element.setAttribute("a", "b");
var attr = element.attributes.item(0);
element = null;

gc();

shouldBe("attr.name", "'a'");
shouldBe("attr.specified", "true");
shouldBe("attr.value", "'b'");
shouldBe("attr.ownerElement.tagName", "'P'");

attr.value = 'c';

shouldBe("attr.value", "'c'");

var successfullyParsed = true;
