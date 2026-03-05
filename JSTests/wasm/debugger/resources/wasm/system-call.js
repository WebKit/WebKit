let iteration = 0;
for (; ;) {
    iteration += 1;
    if (iteration % 1e8 == 0)
        print("iteration=", iteration);
}
