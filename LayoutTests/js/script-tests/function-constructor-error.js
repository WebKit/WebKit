description(
"This test checks that the Function constructor detects some syntax errors correctly (bug#59795)."
);

shouldThrow("Function('(i + (j)')", '"SyntaxError: Expected token \')\'"');
shouldThrow("Function('return (i + (j)')", '"SyntaxError: Expected token \')\'"');
