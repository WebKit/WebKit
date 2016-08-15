// This used to crash because overriding Map as a variable, not property, would trigger a bug in getOwnPropertySlot.

function Map() {
}
