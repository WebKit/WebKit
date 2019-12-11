#include <ck_spinlock.h>

#define LOCK_NAME "ck_ticket"
#define LOCK_DEFINE static ck_spinlock_ticket_t CK_CC_CACHELINE lock = CK_SPINLOCK_TICKET_INITIALIZER
#define LOCK ck_spinlock_ticket_lock(&lock)
#define UNLOCK ck_spinlock_ticket_unlock(&lock)
#ifdef CK_F_SPINLOCK_TICKET_TRYLOCK
#define TRYLOCK ck_spinlock_ticket_trylock(&lock)
#endif
#define LOCKED ck_spinlock_ticket_locked(&lock)

