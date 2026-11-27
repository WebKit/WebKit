// Marker classes are created by name, so that each test load can name them uniquely.
window.markers = [];

function createMarkerClass(className)
{
    return eval(`(class ${className} { })`);
}

function createUnheldMarker(className, label)
{
    let MarkerClass = createMarkerClass(className);
    let marker = new MarkerClass;
    marker.label = label;
    return marker;
}

function createMarker(className, label)
{
    let marker = createUnheldMarker(className, label);
    markers.push(marker);
    return marker;
}
