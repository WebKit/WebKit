description(
"This test checks that the Function constructor detects some syntax errors correctly (bug#59795)."
);

shouldThrow("Function('(i + (j)')", "\"SyntaxError: Unexpected token '}'. Expected ')' to end a compound expression.\"");
shouldThrow("Function('return (i + (j)')", "\"SyntaxError: Unexpected token '}'. Expected ')' to end a compound expression.\"");
