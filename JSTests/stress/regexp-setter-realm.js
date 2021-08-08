let realm = createGlobalObject();
{
    let descriptor = Object.getOwnPropertyDescriptor(realm.RegExp, "multiline");
    descriptor.set.call(realm.RegExp, 42);
}
{
    let descriptor = Object.getOwnPropertyDescriptor(realm.RegExp, "input");
    descriptor.set.call(realm.RegExp, 42);
}
