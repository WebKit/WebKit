function createSelectElementWithTestData() {
    var example = document.createElement("select");
    example.appendChild(new Option("0", "0", false, false));
    example.appendChild(new Option("1", "1", false, false));
    example.appendChild(new Option("2", "2", false, false));
    return example;
}

function deepCopy(selectElement) {
    var copy = [];
    for(var i = 0; i < selectElement.options.length; ++i)
        copy.push(selectElement.options[i].value);
    return copy.join(",");
}

function createOption(value) {
    return new Option(value + "X", value, false, false);
}

function createGroup(value1, value2) {
    var group = document.createElement("optgroup");
    group.appendChild(new Option(value1 + "X", value1, false, false));
    group.appendChild(new Option(value2 + "Y", value2, false, false));
    return group;
}
