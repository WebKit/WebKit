class x {
    get 0 () { }
}

let propertyName = Object.getOwnPropertyDescriptor(x.prototype, "0").get.name;
if (propertyName !== 'get 0')
    throw `Expected getter name 'get 0', got '${propertyName}'`;

