speciesConstructors = [RegExp, Array, Int32Array.__proto__, Map, Set, ArrayBuffer, Promise];

function testSymbolSpeciesOnConstructor(constructor) {
    if (constructor[Symbol.species] !== constructor)
        throw "Symbol.species should return the constructor for " + constructor.name;
    constructor[Symbol.species] = true;
    if (constructor[Symbol.species] !== constructor)
        throw "Symbol.species was mutable " + constructor.name;

    // Symbol.species should be configurable.
    Object.defineProperty(constructor, Symbol.species, { value: true });
}


speciesConstructors.forEach(testSymbolSpeciesOnConstructor);
