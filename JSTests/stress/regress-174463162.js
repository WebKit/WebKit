// $vm.installPropertyInlineCacheClearingWatchpointWithDeadOwner creates the IC
// watchpoint and sets the owner as dead, simulating the state between GC
// marking-end and CodeBlock sweep.

function main() {
    let proto = $vm.createCustomTestGetterSetterWithSharedStructure();
    let sibling = $vm.createCustomTestGetterSetterWithSharedStructure();

    $vm.installPropertyInlineCacheClearingWatchpointWithDeadOwner(proto, "customAccessor");

    sibling.triggerTransition = true;
}
main();
