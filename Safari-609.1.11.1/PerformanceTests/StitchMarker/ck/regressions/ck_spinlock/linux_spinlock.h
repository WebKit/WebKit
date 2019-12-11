#include <ck_cc.h>

CK_CC_INLINE static void
spin_lock(volatile unsigned int *lock)
{
#ifdef __x86_64__
            __asm__ __volatile__(
                    "\n1:\t"
                    "lock ; decl %0\n\t"
                    "jns 2f\n"
                    "3:\n"
                    "rep;nop\n\t"
                    "cmpl $0,%0\n\t"
                    "jle 3b\n\t"
                    "jmp 1b\n"
                    "2:\t" : "=m" (*lock) : : "memory");
#else
	*lock = 1;
#endif

          return;
}

CK_CC_INLINE static void
spin_unlock(volatile unsigned int *lock)
{
#ifdef __x86_64__
        __asm__ __volatile__("movl $1,%0" :"=m" (*lock) :: "memory");
#else
	*lock = 0;
        return;
#endif
}

#define LOCK_NAME "linux_spinlock"
#define LOCK_DEFINE volatile unsigned int lock = 1
#define LOCK spin_lock(&lock)
#define UNLOCK spin_unlock(&lock)

