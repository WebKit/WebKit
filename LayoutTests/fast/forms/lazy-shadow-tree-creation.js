const types = {
    "color": { value: "#008000" },
    "date": { value: "2022-01-01" },
    "datetime-local": { value: "2022-01-01" },
    "email": { value: "hello@example.com" },
    "file": { },
    "month": { value: "2022-01" },
    "number": { value: "123" },
    "password": { },
    "range": { value: "75" },
    "search": { value: "abc" },
    "telephone": { value: "+61300000000" },
    "text": { value: "abc" },
    "time": { value: "10:00" },
    "url": { value: "https://example.com/" },
    "week": { value: "2022-W1" },
};

function supportsType(typeName) {
    let e = document.createElement("input");
    e.type = typeName;
    return e.type == typeName;
}

function makeAndAppendInput(options, typeName, attributes = { }) {
    let span = document.createElement("span");
    document.body.append(span);

    if (options.useParser) {
        let tag = `<input type="${typeName}"`;
        for (let name in attributes)
            tag += ` ${name}="${attributes[name]}"`;
        tag += ">";
        span.innerHTML = tag;
    } else {
        let input = document.createElement("input");
        input.type = typeName;
        for (let name in attributes)
            input.setAttribute(name, attributes[name]);
        span.append(input);
    }
}

function run(options) {
    for (let typeName in types) {
        if (!supportsType(typeName))
            continue;

        let type = types[typeName];
        makeAndAppendInput(options, typeName);

        if (type.value)
            makeAndAppendInput(options, typeName, { value: type.value });

        makeAndAppendInput(options, typeName, { value: type.value, disabled: "disabled" });
    }
}
