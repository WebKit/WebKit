#define LOCK_NAME "ck_mcs"
#define LOCK_DEFINE static ck_spinlock_mcs_t CK_CC_CACHELINE lock = NULL
#define LOCK_STATE ck_spinlock_mcs_context_t node CK_CC_CACHELINE;
#define LOCK ck_spinlock_mcs_lock(&lock, &node)
#define UNLOCK ck_spinlock_mcs_unlock(&lock, &node)
#define LOCKED ck_spinlock_mcs_locked(&lock)

