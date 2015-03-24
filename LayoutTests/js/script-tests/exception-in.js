description("Test for error messages for in");

shouldThrow("20   in   'in in in'  ");
shouldThrow("20   in   true  ");
shouldThrow("20   in   {}.foo  ");
shouldThrow("20   in   20  ");
shouldThrow("20   in   null  ");
