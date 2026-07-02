//@ requireOptions("--useShadowRealm=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

// shadow realm specs
{
    let shadowRealmProp = Object.getOwnPropertyDescriptor(this, "ShadowRealm");
    shouldBe(shadowRealmProp.enumerable, false);
    shouldBe(shadowRealmProp.writable, true);
    shouldBe(shadowRealmProp.configurable, true);

    let shadowRealmEvaluate = Object.getOwnPropertyDescriptor(ShadowRealm.prototype, "evaluate");
    shouldBe(shadowRealmEvaluate.enumerable, false);
    shouldBe(shadowRealmEvaluate.writable, true);
    shouldBe(shadowRealmEvaluate.configurable, true);

    let shadowRealmImportValue = Object.getOwnPropertyDescriptor(ShadowRealm.prototype, "importValue");
    shouldBe(shadowRealmImportValue.enumerable, false);
    shouldBe(shadowRealmImportValue.writable, true);
    shouldBe(shadowRealmImportValue.configurable, true);

    let shadowRealmName = Object.getOwnPropertyDescriptor(ShadowRealm, "name");
    shouldBe(shadowRealmName.value, "ShadowRealm");
    shouldBe(shadowRealmName.enumerable, false);
    shouldBe(shadowRealmName.writable, false);
    shouldBe(shadowRealmName.configurable, true);

    let shadowRealmLength = Object.getOwnPropertyDescriptor(ShadowRealm, "length");
    shouldBe(shadowRealmLength.value, 0);
    shouldBe(shadowRealmLength.enumerable, false);
    shouldBe(shadowRealmLength.writable, false);
    shouldBe(shadowRealmLength.configurable, true);
}
