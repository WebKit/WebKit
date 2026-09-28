// [TEST] Not real code. Demonstrates a bare #ifdef SAR guard with no #else, from rdar://186094248.
//
// The guarded call is dead code once RC_REMOVE_SEC_ACCEL_3 ships -- nothing replaces it, so the
// whole #ifdef/#endif block is simply deleted.

void f(void)
{
#ifdef RC_REMOVE_SEC_ACCEL_3
    legacy_unsafe_call();    // vanilla -- opt-out path for testing, unsafe, no replacement
#endif
    walk();
}
