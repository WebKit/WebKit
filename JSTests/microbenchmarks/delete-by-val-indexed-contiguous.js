//@ skip if $model =~ /^Apple Watch/
//@ $skipModes << :lockdown if $buildType == "debug"

function remove(object, index, value) {
    object[index] = value;
    return delete object[index];
}
noInline(remove);

const object = { };
for (let i = 0; i < 8; ++i)
    object[i] = { };

const value = { };
let removed = 0;
for (let i = 0; i < 4e6; ++i) {
    if (remove(object, i & 7, value))
        ++removed;
}
if (removed !== 4e6)
    throw new Error("bad result: " + removed);
if (Object.keys(object).length)
    throw new Error("bad elements: " + Object.keys(object).join());
