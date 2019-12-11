#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define LOCK_NAME "ck_anderson"
#define LOCK_DEFINE static ck_spinlock_anderson_t lock CK_CC_CACHELINE
#define LOCK_STATE ck_spinlock_anderson_thread_t *nad = NULL
#define LOCK ck_spinlock_anderson_lock(&lock, &nad)
#define UNLOCK ck_spinlock_anderson_unlock(&lock, nad)
#define LOCK_INIT ck_spinlock_anderson_init(&lock, malloc(MAX(64,sizeof(ck_spinlock_anderson_thread_t)) * nthr), nthr)
#define LOCKED ck_spinlock_anderson_locked(&lock)

#define NO_LOCAL

