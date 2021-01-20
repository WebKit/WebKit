description(
"This tests that throwing from a finally block has the expected effect."
);

var events = [];

try {
    events.push("1:try");
} finally {
    events.push("1:finally");
}

try {
    try {
        throw "2:thingy";
    } finally {
        events.push("2:finally");
    }
} catch (e) {
    events.push(e);
}

try {
    throw "3:thingy";
} catch (e) {
    events.push(e);
} finally {
    events.push("3:finally");
}

try {
    try {
        throw "4:thingy";
    } catch (e) {
        events.push(e);
    } finally {
        events.push("4:finally");
        throw "4:another thingy";
    }
} catch (e) {
    events.push(e);
}

try {
    for (;;) {
        try {
            continue;
        } finally {
            events.push("5:hi");
            throw "5:wat";
        }
    }
} catch (e) {
    events.push(e);
}

shouldBe("\"\" + events", "\"1:try,1:finally,2:finally,2:thingy,3:thingy,3:finally,4:thingy,4:finally,4:another thingy,5:hi,5:wat\"");

