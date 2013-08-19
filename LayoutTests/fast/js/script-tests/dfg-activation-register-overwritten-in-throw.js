description(
"This tests that the DFG does not attempt to overwrite the activation register with undefined."
);

function g() { 
    (eval("-7") = 0);
}
 
while (!dfgCompiled({f:g})) {
    try {
        g()
    } catch(e) {

    }
}
