const object = JSON.rawJSON('"a"');
for (const name in object) {
    object[name] = 0x1234;
}
JSON.stringify(object);

