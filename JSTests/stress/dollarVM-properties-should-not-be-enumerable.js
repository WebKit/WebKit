//@ runDefault

const descriptors = Object.getOwnPropertyDescriptors($vm);

var success = true;

for (prop in descriptors) {
    let descriptor = descriptors[prop];
    var expected = !descriptor.configurable && !descriptor.enumerable && !descriptor.writable;
    if (!expected) {
        print(" --- " + prop + " --- ", descriptors[prop]);
        if (descriptor.configurable)
            print("    $vm." + prop + " should not be configurable.");
        if (descriptor.enumerable)
            print("    $vm." + prop + " should not be enumerable.");
        if (descriptor.writable)
            print("    $vm." + prop + " should not be writable.");
    }
    success = success && !descriptor.configurable && !descriptor.enumerable && !descriptor.writable;
}

for (prop in $vm) {
    print("$vm." + prop + " should not be enumerable.");
    success = false;
}
    
if (!success)
    throw "FAILED";
