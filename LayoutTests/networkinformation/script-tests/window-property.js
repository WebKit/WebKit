description('Tests that the window.navigator.connection properties are present.');

function hasOnConnectionProperty()
{
    var result = 0;
    for (var property in navigator.webkitConnection) {
        if (property == 'onwebkitnetworkinfochange')
            result += 1;
    }
    if (result == 1)
        return true;
    return false;
}

shouldBeTrue("typeof navigator.webkitConnection == 'object'");
shouldBeTrue("hasOnConnectionProperty()");
shouldBeTrue("navigator.webkitConnection.hasOwnProperty('onwebkitnetworkinfochange')");
