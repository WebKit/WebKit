function get(value, prop) { return value[prop]; }
noInline(get);

function foo(record, key, attribute) {
    var attrs = get(this, 'attrs');
    var value = get(record, key), type = attribute.type;

    if (type) {
        var transform = this.transformFor(type);
        value = transform.serialize(value);
    }

    key = attrs && attrs[key] || (this.keyForAttribute ? this.keyForAttribute(key) : key);

    return {key:key, value:value};
}
noInline(foo);

let i = 0;
let thisValue = {transformFor: function() { return {serialize: function() { return {} }}}};
let record = {key: "hello"};
let record2 = {key: true};
let key = "key";
let attribute = {type: "type"};
for (; i < 100000; i++) {
    if (i % 2 === 0)
        foo.call(thisValue, record, key, attribute);
    else
        foo.call(thisValue, record2, key, attribute);
}
