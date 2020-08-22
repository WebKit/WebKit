//@ skip if ["arm", "mips"].include?($architecture)
function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

let constructors = [
    Intl.Collator,
    Intl.DateTimeFormat,
    Intl.DisplayNames,
    Intl.NumberFormat,
    Intl.PluralRules,
    Intl.RelativeTimeFormat,
];

for (let constructor of constructors) {
    let array = constructor.supportedLocalesOf("en");
    for (let index = 0; index < array.length; ++index) {
        let descriptor = Reflect.getOwnPropertyDescriptor(array, index);
        shouldBe(descriptor.writable, true);
        shouldBe(descriptor.configurable, true);
    }
    let descriptor = Reflect.getOwnPropertyDescriptor(array, "length");
    shouldBe(descriptor.writable, true);
    shouldBe(descriptor.configurable, false);
}
