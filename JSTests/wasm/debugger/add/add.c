int add(int a, int b) {
    int result = a + b;
    return result;
}

int main() {
    int x = 10;
    int y = 20;
    int sum = add(x, y);
    return sum;
}

// emcc -g -O0 -s STANDALONE_WASM=1 -s EXPORTED_FUNCTIONS='["_main","_add"]' add.c -o add.wasm && wasm2wat add.wasm -o add.wat && xcrun wasm-objdump -d add.wasm &> ./wasm-objdump-code-section.txt && xcrun llvm-dwarfdump add.wasm &> ./llvm-dwarfdump-debug-info.txt