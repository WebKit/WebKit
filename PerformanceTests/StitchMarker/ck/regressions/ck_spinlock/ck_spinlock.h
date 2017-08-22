#define LOCK_NAME "ck_spinlock"
#define LOCK_DEFINE static ck_spinlock_t CK_CC_CACHELINE lock = CK_SPINLOCK_INITIALIZER
#define LOCK ck_spinlock_lock_eb(&lock)
#define UNLOCK ck_spinlock_unlock(&lock)
#define LOCKED ck_spinlock_locked(&lock)

