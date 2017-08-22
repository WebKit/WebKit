#define LOCK_NAME "ck_dec"
#define LOCK_DEFINE static ck_spinlock_dec_t CK_CC_CACHELINE lock = CK_SPINLOCK_DEC_INITIALIZER
#define LOCK ck_spinlock_dec_lock_eb(&lock)
#define UNLOCK ck_spinlock_dec_unlock(&lock)
#define LOCKED ck_spinlock_dec_locked(&lock)

