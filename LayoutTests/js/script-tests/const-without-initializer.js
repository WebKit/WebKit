description(
'Tests that declaring a const variable without initializing has the correct behavior and does not crash'
);

shouldThrow("const f");
