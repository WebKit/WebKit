function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe(["[Object object]", "[Object object]", "[Object object]", "[Object object]", "[Object object]", "[Object object]"].toString(), `[Object object],[Object object],[Object object],[Object object],[Object object],[Object object]`);
shouldBe(["Object ", "Object ", "Object ", "Object ", "Object ", "Object ", "Object ", "Object ", "Object "].toString(), `Object ,Object ,Object ,Object ,Object ,Object ,Object ,Object ,Object `);
shouldBe(["Obj", "Obj", "Obj", "Obj", "Obj", "Obj", "Obj", "Obj", "Obj", "Obj", "Obj"].toString(), `Obj,Obj,Obj,Obj,Obj,Obj,Obj,Obj,Obj,Obj,Obj`);
