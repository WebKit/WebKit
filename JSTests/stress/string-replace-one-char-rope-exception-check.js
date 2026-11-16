const floatArray = new Float32Array(211);
for (const key in floatArray) {
    for (let i = 0; i < 10; i++)
        (floatArray + i).replace(key, "p");
}
