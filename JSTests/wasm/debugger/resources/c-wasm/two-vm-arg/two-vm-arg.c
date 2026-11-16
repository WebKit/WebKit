// WASM Generation:
//   emcc -g -O0 -s STANDALONE_WASM=1 --no-entry -s EXPORTED_FUNCTIONS='["_compute"]' \
//        two-vm-arg.c -o two-vm-arg.wasm
volatile int sink;

int add(int a, int b)
{
    return a + b;
}

int compute(int thread, int iteration)
{
    int a = thread;
    int b = iteration;
    sink = add(a, b);
    return thread;
}
