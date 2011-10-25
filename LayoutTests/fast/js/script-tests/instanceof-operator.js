description(
"instanceof test"
);

getterCalled = false;
try {
    ({} instanceof { get prototype(){ getterCalled = true; } });
} catch (e) {
}
shouldBeFalse("getterCalled");
