description(
"This tests that the DFG does not attempt to overwrite the activation register with undefined."
);

function g() { 
    (eval("-7") = 0);
}
 
for (var i = 0; i < 10000; i++) {
    try {
        g()
    } catch(e) {

    }
}
