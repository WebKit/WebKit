let toObjectablePrimitives = [true, false, 1, 2, "", Symbol(), BigInt(1)];

for (let primitive of toObjectablePrimitives)
    Object.create({}, primitive);

function shouldThrow(props) {
    try {
        Object.create({}, props);
    } catch (e) {
        if (!(e instanceof TypeError))
            throw e;
    }
}

shouldThrow("hello");
shouldThrow(null);
