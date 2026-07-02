function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

let obj = {
    1: "v0",
    20230726: "v1",
    20230802: "v2",
    20230809: "v3",
    20230816: "v4",
    20230823: "v5",
    20230830: "v6",
    20230906: "v7",
    20230913: "v8",
    20230920: "v9",
    20230927: "v10",
    20231004: "v11",
    20231011: "v12",
    20231018: "v13",
    20231025: "v14",
};

{
    let actual = Object.values(obj).toString();
    let expected = "v0,v1,v2,v3,v4,v5,v6,v7,v8,v9,v10,v11,v12,v13,v14";
    shouldBe(actual, expected);
}

{
    let actual = JSON.stringify(Object.assign({}, obj));
    let expected = `{"1":"v0","20230726":"v1","20230802":"v2","20230809":"v3","20230816":"v4","20230823":"v5","20230830":"v6","20230906":"v7","20230913":"v8","20230920":"v9","20230927":"v10","20231004":"v11","20231011":"v12","20231018":"v13","20231025":"v14"}`;
    shouldBe(actual, expected);
}
