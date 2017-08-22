#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define LOCK_NAME "ck_clh"
#define LOCK_DEFINE static ck_spinlock_hclh_t CK_CC_CACHELINE *glob_lock; \
		    static ck_spinlock_hclh_t CK_CC_CACHELINE *local_lock[CORES / 2]
#define LOCK_STATE ck_spinlock_hclh_t *na = malloc(MAX(sizeof(ck_spinlock_hclh_t), 64))
#define LOCK ck_spinlock_hclh_lock(&glob_lock, &local_lock[(core % CORES) / 2], na)
#define UNLOCK ck_spinlock_hclh_unlock(&na)
#define LOCK_INIT do {							\
	int _i;								\
	ck_spinlock_hclh_init(&glob_lock, malloc(MAX(sizeof(ck_spinlock_hclh_t), 64)), -1); \
	for (_i = 0; _i < CORES / 2; _i++) {				\
		ck_spinlock_hclh_init(&local_lock[_i], malloc(MAX(sizeof(ck_spinlock_hclh_t), 64)), _i);	} \
} while (0)

#define LOCKED ck_spinlock_hclh_locked(&glob_lock)

