function main() {
    for (let v27 = 0; v27 < 100; v27++) {
        const v44 = [0,0,1.1];
        const v61 = v44.toLocaleString();
        const v62 = eval(Math);
        v63 = v61.substring(v62,v27);

        function v64() {
            if (v62) {
                Math[v61] = [];
            }
            const v82 = (-1.0).__proto__;
            delete v82[v63];
        }
        v64();
    }
}
main();
