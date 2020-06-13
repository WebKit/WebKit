//@ runDefault("--validateBytecode=1")
try {
    x||=y()=0
    x||=(y()++)
    x||=(++y())
    x||=(y()+=0)
    x||=(y()||=0)
} catch(e) {
}
