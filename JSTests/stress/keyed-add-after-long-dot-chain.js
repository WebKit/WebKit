function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected " + expected);
}

function structureID(object) {
    const list = $vm.getStructureTransitionList(object);
    return list[list.length - 5];
}

const fieldCount = 200;
let body = "";
for (let i = 0; i < fieldCount; ++i)
    body += "this.f" + i + " = " + i + ";\n";
const Big = new Function(body);

function addKeyed(object, prefix, count) {
    for (let i = 0; i < count; ++i)
        object[prefix + i] = i * 10;
}

{
    const a = new Big();
    const b = new Big();
    addKeyed(a, "extra", 20);
    addKeyed(b, "extra", 20);
    if (structureID(a) !== structureID(b))
        throw new Error("a few keyed adds on a large dot-syntax object should keep sharing transitions");
}

{
    const a = new Big();
    const b = new Big();
    addKeyed(a, "many", 100);
    addKeyed(b, "many", 100);
    if (structureID(a) === structureID(b))
        throw new Error("keyed adds past the bound should turn each instance into a dictionary");
    shouldBe(a.many99, 990);
    shouldBe(a.f199, 199);
}

{
    const a = {};
    const b = {};
    addKeyed(a, "key", 200);
    addKeyed(b, "key", 200);
    if (structureID(a) === structureID(b))
        throw new Error("a keyed-only object past the limit should become a dictionary");
    shouldBe(a.key199, 1990);
}

function readExtra(object, i) { return object["extra" + i]; }
function readField(object) { return object.f150; }
function readFirstExtra(object) { return object.extra0; }
noInline(readExtra);
noInline(readField);
noInline(readFirstExtra);

const objects = [];
for (let n = 0; n < 50; ++n) {
    const object = new Big();
    addKeyed(object, "extra", 20);
    objects.push(object);
}
for (let n = 1; n < objects.length; ++n)
    shouldBe(structureID(objects[n]), structureID(objects[0]));

for (let iteration = 0; iteration < testLoopCount; ++iteration) {
    const object = objects[iteration % objects.length];
    shouldBe(readField(object), 150);
    shouldBe(readFirstExtra(object), 0);
    shouldBe(readExtra(object, iteration % 20), (iteration % 20) * 10);
}
