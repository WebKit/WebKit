function hypot4(arg1, arg2, arg3, arg4) {
    return Math.hypot(arg1, arg2, arg3, arg4);
}

function hypot5(arg1, arg2, arg3, arg4, arg5) {
    return Math.hypot(arg1, arg2, arg3, arg4, arg5);
}
noInline(hypot4);
noInline(hypot5);

for (var i = 0; i < testLoopCount; ++i) {
    hypot4(3, 4, 6, 45);
    hypot4(9, 12, 0.4, -3);
    hypot5(400.2, -3.4, 3, 10, -45);
    hypot5(-24.3, -400.2, -0.4, 3, 2);
}
