function testParse(declaration) {
    var div = document.createElement("div");
    div.setAttribute("style", declaration);
    return div.style.webkitFlowInto;
}

function testComputedStyle(value) {
    var div = document.createElement("div");
    document.body.appendChild(div);
    div.style.setProperty("-webkit-flow-into", value);
    var webkitFlowComputedValue = getComputedStyle(div).getPropertyValue("-webkit-flow-into");
    document.body.removeChild(div);
    return webkitFlowComputedValue;
}

function testNotInherited(parentValue, childValue) {
    var parentDiv = document.createElement("div");
    document.body.appendChild(parentDiv);
    parentDiv.style.setProperty("-webkit-flow-into", parentValue);

    var childDiv = document.createElement("div");
    parentDiv.appendChild(childDiv);
    childDiv.style.setProperty("-webkit-flow-into", childValue);

    var childWebKitFlowComputedValue = getComputedStyle(childDiv).getPropertyValue("-webkit-flow-into");

    parentDiv.removeChild(childDiv);
    document.body.removeChild(parentDiv);

    return childWebKitFlowComputedValue;
}

test(function() {assert_equals(testParse("-webkit-flow-into: none"), "none")}, "Test Parse none"); 
test(function() {assert_equals(testParse("-webkit-flow-into: first-flow"), "first-flow")}, "Test Parse first-flow");
test(function() {assert_equals(testParse("-webkit-flow-into: \'first flow\'"), "")}, "Test Parse 'first-flow'");
test(function() {assert_equals(testParse("-webkit-flow-into: ;"), "")}, "Test Parse ;");
test(function() {assert_equals(testParse("-webkit-flow-into: 1"), "")}, "Test Parse 1");
test(function() {assert_equals(testParse("-webkit-flow-into: 1.2"), "")}, "Test Parse 1.2");
test(function() {assert_equals(testParse("-webkit-flow-into: -1"), "")}, "Test Parse -1");
test(function() {assert_equals(testParse("-webkit-flow-into: 12px"), "")}, "Test Parse 12px");

test(function() {assert_equals(testComputedStyle("none"), "none")}, "Test Computed Style none");
test(function() {assert_equals(testComputedStyle(""), "none")}, "Test Computed Style ''");
test(function() {assert_equals(testComputedStyle("\'first-flow\'"), "none")}, "Test Computed Style 'first-flow'");
test(function() {assert_equals(testComputedStyle("first-flow"), "first-flow")}, "Test Computed Style first-flow");
test(function() {assert_equals(testComputedStyle("12px"), "none")}, "Test Computed Style 12px ");

test(function() {assert_equals(testNotInherited("none", "none"), "none")}, "Test Non-Inherited none, none");
test(function() {assert_equals(testNotInherited("none", "child-flow"), "child-flow")}, "Test Non-Inherited none, child-flow");
test(function() {assert_equals(testNotInherited("parent-flow", "none"), "none")}, "Test Non-Inherited parent-flow, none");
test(function() {assert_equals(testNotInherited("parent-flow", "child-flow"), "child-flow")}, "Test Non-Inherited parent-flow, child-flow");
