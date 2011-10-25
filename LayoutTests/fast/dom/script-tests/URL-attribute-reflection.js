description("Test reflecting URL attributes with empty string values.");

function testURLReflection(attributeName, tag, scriptAttributeName)
{
    if (!scriptAttributeName)
        scriptAttributeName = attributeName.toLowerCase();

    var element;
    if (tag === "html")
        element = document.documentElement;
    else {
        element = document.createElement(tag);
        document.body.appendChild(element);
    }
    element.setAttribute(scriptAttributeName, "x");
    var xValue = element[attributeName];
    element.setAttribute(scriptAttributeName, "");
    var emptyValue = element[attributeName];
    if (tag !== "html")
        document.body.removeChild(element);

    if (xValue === undefined)
        return "none";
    if (xValue === "x")
        return "non-URL";
    if (xValue !== document.baseURI.replace(/[^\/]+$/, "x"))
        return "error (x): " + xValue;
    if (emptyValue === "")
        return "non-empty URL";
    if (emptyValue === document.baseURI)
        return "URL";
    return "error (empty): " + emptyValue;
}

shouldBe("testURLReflection('attribute', 'element')", "'none'");
shouldBe("testURLReflection('id', 'element')", "'non-URL'");

// The following list comes from the HTML5 document’s attributes index.
// These are the URL attributes from that list.

shouldBe("testURLReflection('action', 'form')", "'URL'");
shouldBe("testURLReflection('cite', 'blockquote')", "'URL'");
shouldBe("testURLReflection('cite', 'del')", "'URL'");
shouldBe("testURLReflection('cite', 'ins')", "'URL'");
shouldBe("testURLReflection('cite', 'q')", "'URL'");
shouldBe("testURLReflection('data', 'object')", "'URL'");
shouldBe("testURLReflection('formaction', 'button')", "'URL'");
shouldBe("testURLReflection('formaction', 'input')", "'URL'");
shouldBe("testURLReflection('href', 'a')", "'URL'");
shouldBe("testURLReflection('href', 'area')", "'URL'");
shouldBe("testURLReflection('href', 'link')", "'URL'");
shouldBe("testURLReflection('href', 'base')", "'URL'");
shouldBe("testURLReflection('icon', 'command')", "'URL'");
shouldBe("testURLReflection('manifest', 'html')", "'URL'");
shouldBe("testURLReflection('poster', 'video')", "'URL'");
shouldBe("testURLReflection('src', 'audio')", "'URL'");
shouldBe("testURLReflection('src', 'embed')", "'URL'");
shouldBe("testURLReflection('src', 'iframe')", "'URL'");
shouldBe("testURLReflection('src', 'img')", "'URL'");
shouldBe("testURLReflection('src', 'input')", "'URL'");
shouldBe("testURLReflection('src', 'script')", "'URL'");
shouldBe("testURLReflection('src', 'source')", "'URL'");
shouldBe("testURLReflection('src', 'video')", "'URL'");

// Other reflected URL attributes.

shouldBe("testURLReflection('longDesc', 'img')", "'URL'");
shouldBe("testURLReflection('lowsrc', 'img')", "'URL'");
