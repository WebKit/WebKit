// This should not crash the parser.
function main() {
    class a {
        g = [].toString()
        'a'(){}
    }
}
