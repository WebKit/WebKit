function hypot8(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) {
    return Math.hypot(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
}

function hypot9(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9) {
    return Math.hypot(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
}
noInline(hypot8);
noInline(hypot9);

for (var i = 0; i < testLoopCount; ++i) {
    hypot8(3, 4, 6, 45, 0.554, 45, 12343, -6543);
    hypot8(9, 12, 0.4, -3, 1000, -0.34, -432234, 9432);
    hypot9(400.2, -3.4, 3, 10, -45, 1212, 1919, 90865, 345);
    hypot9(-24.3, -400.2, -0.4, 3, 2, 1072, -0.231, -345, -543.342);
}
