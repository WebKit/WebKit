function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    let a = {
        get cocoa() {
            return "Cocoa";
        },

        get cappuccino() {
            return "Cappuccino";
        }
    }

    shouldBe(JSON.stringify(a), `{"cocoa":"Cocoa","cappuccino":"Cappuccino"}`);
    shouldBe(JSON.stringify(a, ["cocoa", "cappuccino"]), `{"cocoa":"Cocoa","cappuccino":"Cappuccino"}`);
}
{
    let a = {
        get cocoa() {
            Reflect.defineProperty(a, "cappuccino", { value: 42 });
            return "Cocoa";
        },

        get cappuccino() {
            throw new Error("out");
        }
    }

    shouldBe(JSON.stringify(a), `{"cocoa":"Cocoa","cappuccino":42}`);
}
{
    let a = {
        get cocoa() {
            Reflect.defineProperty(a, "next", { value: 42 });
            return "Cocoa";
        },

        get cappuccino() {
            return "Cappuccino";
        }
    }

    shouldBe(JSON.stringify(a), `{"cocoa":"Cocoa","cappuccino":"Cappuccino"}`);
}
{
    let a = {
        get cocoa() {
            Reflect.deleteProperty(a, "cappuccino");
            return "Cocoa";
        },

        get cappuccino() {
            return "Cappuccino";
        }
    }

    shouldBe(JSON.stringify(a), `{"cocoa":"Cocoa"}`);
}
{
    let a = {
        get cocoa() {
            return "Cocoa";
        },

        set cappuccino(value) {
            throw new Error("out");
        }
    }

    shouldBe(JSON.stringify(a), `{"cocoa":"Cocoa"}`);
}
