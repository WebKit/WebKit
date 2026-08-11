function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

let object = {
    a: 42,
    b: 44,
    c: 43,
    d: 44,
};

function collect(object)
{
    let array = [];
    for (let key in object)
        array.push(key);
    return array;
}

let array1 = collect(object);
let array2 = collect(object);
$vm.toCacheableDictionary(object);
object.e = 44;
let array3 = collect(object);
let array4 = collect(object);
object.f = 44;
let array5 = collect(object);

shouldBe(JSON.stringify(array1), `["a","b","c","d"]`);
shouldBe(JSON.stringify(array2), `["a","b","c","d"]`);
shouldBe(JSON.stringify(array3), `["a","b","c","d","e"]`);
shouldBe(JSON.stringify(array4), `["a","b","c","d","e"]`);
shouldBe(JSON.stringify(array5), `["a","b","c","d","e","f"]`);
