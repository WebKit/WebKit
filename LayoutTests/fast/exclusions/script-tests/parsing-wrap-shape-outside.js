description('Testing the parsing of the -webkit-wrap-shape-outside property.');

function testCSSText(declaration, expected)
{
    var element = document.createElement("div");
    element.style.cssText = "-webkit-wrap-shape-outside: " + declaration;
    return element.style.webkitWrapShapeOutside;
}

function testComputedStyle(value, expected) {
    var element = document.createElement("div");
    document.body.appendChild(element);
    element.style.setProperty("-webkit-wrap-shape-outside", value);

    var computedStyle = getComputedStyle(element);
    var actualValue = computedStyle.getPropertyValue("-webkit-wrap-shape-outside");
    document.body.removeChild(element);

    return actualValue;
}

function testNotInherited(parentValue, childValue) {
   var parentElement = document.createElement("div");
   document.body.appendChild(parentElement);
   parentElement.style.setProperty("-webkit-wrap-shape-outside", parentValue);

   var childElement = document.createElement("div");
   parentElement.appendChild(childElement);
   childElement.style.setProperty("-webkit-wrap-shape-outside", childValue);

   var parentComputedStyle = getComputedStyle(parentElement);
   var parentActual = parentComputedStyle.getPropertyValue('-webkit-wrap-shape-outside')

   var childComputedStyle = getComputedStyle(childElement);
   var childActual = childComputedStyle.getPropertyValue('-webkit-wrap-shape-outside')

   parentElement.removeChild(childElement);
   document.body.removeChild(parentElement);

   return "parent: " + parentActual + ", child: " + childActual;
}

function test(value, expected) {
    var unevaledString = '"' + value.replace(/\\/g, "\\\\").replace(/"/g, "\"").replace(/\n/g, "\\n").replace(/\r/g, "\\r") + '"';
    shouldBeEqualToString('testCSSText(' + unevaledString + ')', expected);
    shouldBeEqualToString('testComputedStyle(' + unevaledString + ')', expected);
}

function negative_test(value) {
    var unevaledString = '"' + value.replace(/\\/g, "\\\\").replace(/"/g, "\"").replace(/\n/g, "\\n").replace(/\r/g, "\\r") + '"';
    shouldBeEqualToString('testCSSText(' + unevaledString + ')', '');
    shouldBeEqualToString('testComputedStyle(' + unevaledString + ')', 'auto');
}

// positive tests

test("auto", "auto");
test("rect(10px, 20px, 30px, 40px)", "rect(10px, 20px, 30px, 40px)");
test("rect(10px, 20px, 30px, 40px, 5px)", "rect(10px, 20px, 30px, 40px, 5px)");
test("rect(10px, 20px, 30px, 40px, 5px, 10px)", "rect(10px, 20px, 30px, 40px, 5px, 10px)");

test("circle(10px, 20px, 30px)", "circle(10px, 20px, 30px)");

test("ellipse(10px, 20px, 30px, 40px)", "ellipse(10px, 20px, 30px, 40px)");

test("polygon(10px, 20px 30px, 40px 40px, 50px)", "polygon(nonzero, 10px, 20px 30px, 40px 40px, 50px)");
test("polygon(evenodd, 10px, 20px 30px, 40px 40px, 50px)", "polygon(evenodd, 10px, 20px 30px, 40px 40px, 50px)");
test("polygon(nonzero, 10px, 20px 30px, 40px 40px, 50px)", "polygon(nonzero, 10px, 20px 30px, 40px 40px, 50px)");

shouldBeEqualToString('testNotInherited("auto", "rect(10px, 20px, 30px, 40px)")', "parent: auto, child: rect(10px, 20px, 30px, 40px)");
shouldBeEqualToString('testNotInherited("rect(10px, 20px, 30px, 40px)", "initial")', "parent: rect(10px, 20px, 30px, 40px), child: auto");
shouldBeEqualToString('testNotInherited("rect(10px, 20px, 30px, 40px)", "")', "parent: rect(10px, 20px, 30px, 40px), child: auto");
shouldBeEqualToString('testNotInherited("rect(10px, 20px, 30px, 40px)", "inherit")', "parent: rect(10px, 20px, 30px, 40px), child: rect(10px, 20px, 30px, 40px)");
shouldBeEqualToString('testNotInherited("", "inherit")', "parent: auto, child: auto");
shouldBeEqualToString('testNotInherited("auto", "inherit")', "parent: auto, child: auto");

// negative tests

negative_test("calc()");
negative_test("none");

negative_test("rect()");
negative_test("rect(10px)");
negative_test("rect(10px, 10px)");
negative_test("rect(10px, 20px, 30px)");
negative_test("rect(10px 20px 30px 40px)");
negative_test("rect(10px, 20px, 30px, 40px, 50px, 60px, 70px)");

negative_test("circle()");
negative_test("circle(10px)");
negative_test("circle(10px, 20px)");
negative_test("circle(10px 20px 30px)");
negative_test("circle(10px, 20px, 30px, 40px)");

negative_test("ellipse()");
negative_test("ellipse(10px)");
negative_test("ellipse(10px, 20px)");
negative_test("ellipse(10px, 20px, 30px)");
negative_test("ellipse(10px 20px 30px 40px)");

negative_test("polygon()");
negative_test("polygon(evenodd 10px, 20px 30px, 40px 40px, 50px)");
negative_test("polygon(nonzero 10px, 20px 30px, 40px 40px, 50px)");
negative_test("polygon(nonzero)");
negative_test("polygon(evenodd)");
negative_test("polygon(10px)");
negative_test("polygon(nonzero,10px)");
negative_test("polygon(evenodd,12px)");
negative_test("polygon(10px, 20px, 30px, 40px, 40px, 50px)");
