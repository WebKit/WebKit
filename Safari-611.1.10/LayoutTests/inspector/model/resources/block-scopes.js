function testBlockScopes() {
    // Normal (not Block)
    let a = 1;
    console.log("normal", a);
    debugger; // 1
    
    // If
    if (true) {
        let a = 990;
        console.log("if", a);
        debugger; // 2
    }

    // Else
    if (false) {}
    else {
        let a = 991;
        console.log("else", a);
        debugger; // 3
    }

    // While
    while (true) {
        let a = 992;
        console.log("while", a);
        debugger; // 4
        break;
    }

    // Do/While
    do {
        let a = 993;
        console.log("do", a);
        debugger; // 5
    } while (false);

    // For
    for (let a = 994; true; ++a) {
        console.log("for", a);
        debugger; // 6
        break;
    }

    // ForIn
    for (let a in ({"995":1})) {
        console.log("for-in", a);
        debugger; // 7
        break;
    }

    // ForOf
    for (let a of [996]) {
        console.log("for-of", a);
        debugger; // 8
        break;
    }

    // Switch
    switch ("test") {
    case "test":
        let a = 997;
        console.log("case", a);
        debugger; // 9
        // fallthrough
    default:
        a += 0.5;
        console.log("default", a);
        debugger; // 10
        break;
    }

    // Try
    try {
        let a = 998;
        console.log("try", a);
        debugger; // 11
    } catch (e) {}

    // Catch
    try {
        throw "Error";
    } catch (e) {
        let a = 999;
        console.log("catch", a);
        debugger; // 12
    }

    // Finally
    try {}
    finally {
        let a = 1000;
        console.log("finally", a);
        debugger; // 13
    }

    // Anonymous block.
    {
        let a = 1001;
        console.log("block", a);
        debugger; // 14
    }

    // With block.
    with ({something:true,__proto__:null}) {
        let a = 1002;
        console.log("with", a);
        debugger; // 15
    }

    // Nested blocks.
    {
        let a = 1003;
        {
            let a = 1004;
            {
                let a = 1005;
                {
                    let a = 1006;
                    console.log("nested blocks", a);
                    debugger; // 16
                }
            }
        }
    }

    // Class (method)
    (new (class MyClass {
        method() {
            // Block Variable scope containing the Class identifier `MyClass`.
            console.log("class (method)");
            debugger; // 17
        }
    })).method();

    // Class (static method)
    (class MyClassWithStaticMethod {
        static staticMethod() {
            // Block Variable scope containing the Class identifier `MyClass`.
            console.log("class (static)");
            debugger; // 18
        }
    }).staticMethod();

    // Normal (not Block)
    console.log("normal", a);
    debugger; // 19
}
