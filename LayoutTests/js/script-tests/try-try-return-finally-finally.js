description(
"Tests what would happen if you have nested try-finally's with interesting control statements nested within them. The correct outcome is for this test to not crash during bytecompilation."
);

function foo() {
    try{
        while(a){
            try{
                if(b){return}
            }finally{
                c();
            }
            if(d){return}
        }
    }finally{
        e();
    }
}

try {
    foo();
} catch (e) {
    testPassed("It worked.");
}

