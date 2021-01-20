description("Test to make sure we don't automatically insert semicolons at the end of a script.");

shouldThrow("if (0)");
shouldThrow("eval('if (0)')");
