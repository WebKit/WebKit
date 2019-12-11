#define LOCK_NAME "ck_cas"
#define LOCK_DEFINE static ck_spinlock_cas_t CK_CC_CACHELINE lock = CK_SPINLOCK_CAS_INITIALIZER
#define LOCK ck_spinlock_cas_lock_eb(&lock)
#define UNLOCK ck_spinlock_cas_unlock(&lock)
#define LOCKED ck_spinlock_cas_locked(&lock)

