try {
    new WebAssembly.Memory({initial : 0, maximum : 1, shared : true}).grow(1)
} catch {
}
