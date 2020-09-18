//@requireOptions("--watchdog=5000", "--watchdog-exception-ok", "--jitPolicyScale=0")
function build_array(res){
    for(let i = 0; i < 1; i++){
        res[0] = i;
    }
}

function main() {
        var v6 = 0;
        var v3 = new Int32Array(1);
        var v4 = new Int32Array(1);    
        while (1) {
            build_array([1]);
            v4[v6] = v3[v6];
            v6++;
        }
}
main();
