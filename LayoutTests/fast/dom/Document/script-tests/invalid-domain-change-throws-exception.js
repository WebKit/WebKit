description("This test checks that a SECURITY_ERR exception is raised if an attempt is made to change document.domain to an invalid value.");

shouldThrow('document.domain = "apple.com"', '"SecurityError (DOM Exception 18): The operation is insecure."');
