description("This test checks that a SECURITY_ERR exception is raised if an attempt is made to change document.domain to an invalid value.");

shouldThrow('document.domain = "apple.com"', '"Error: SECURITY_ERR: DOM Exception 18"');

var successfullyParsed = true;
