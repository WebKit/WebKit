#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define LOCK_NAME "ck_clh"
#define LOCK_DEFINE static ck_spinlock_clh_t CK_CC_CACHELINE *lock = NULL
#define LOCK_STATE ck_spinlock_clh_t *na = malloc(MAX(sizeof(ck_spinlock_clh_t), 64))
#define LOCK ck_spinlock_clh_lock(&lock, na)
#define UNLOCK ck_spinlock_clh_unlock(&na)
#define LOCK_INIT ck_spinlock_clh_init(&lock, malloc(MAX(sizeof(ck_spinlock_clh_t), 64)))
#define LOCKED ck_spinlock_clh_locked(&lock)

