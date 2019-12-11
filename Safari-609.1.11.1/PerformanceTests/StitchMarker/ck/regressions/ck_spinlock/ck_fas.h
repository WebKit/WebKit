#define LOCK_NAME "ck_fas"
#define LOCK_DEFINE static ck_spinlock_fas_t CK_CC_CACHELINE lock = CK_SPINLOCK_FAS_INITIALIZER
#define LOCK ck_spinlock_fas_lock_eb(&lock)
#define UNLOCK ck_spinlock_fas_unlock(&lock)
#define LOCKED ck_spinlock_fas_locked(&lock)

