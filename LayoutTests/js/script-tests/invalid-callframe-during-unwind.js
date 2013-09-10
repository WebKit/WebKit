description("Test to ensure we have a valid callframe midway through unwinding");

function testUnwind(){with({}){ arguments; throw "threw successfully";}}

shouldThrow("testUnwind()")
