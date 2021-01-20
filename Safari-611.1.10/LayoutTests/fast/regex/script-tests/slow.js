description(
'Test for expressions that would hang when evaluated due to exponential matching behavior. If the test does not hang it is a success.'
);

shouldBe('/(?:[^(?!)]||){23}z/.test("/(?:[^(?!)]||){23}z/")', 'false');
