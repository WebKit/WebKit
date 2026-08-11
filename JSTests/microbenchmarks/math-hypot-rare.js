function hypot6(arg1, arg2, arg3, arg4, arg5, arg6) {
    return Math.hypot(arg1, arg2, arg3, arg4, arg5, arg6);
}

function hypot7(arg1, arg2, arg3, arg4, arg5, arg6, arg7) {
    return Math.hypot(arg1, arg2, arg3, arg4, arg5, arg6, arg7);
}
noInline(hypot6);
noInline(hypot7);

for (var i = 0; i < testLoopCount; ++i) {
    hypot6(3, 4, 6, 45, 0.554, 12344);
    hypot6(9, 12, 0.4, -3, 1000, 12334);
    hypot7(400.2, -3.4, 3, 10, -45, 1212, 1919);
    hypot7(-24.3, -400.2, -0.4, 3, 2, 1072, -0.231);
}
