DIR=array	\
    backoff	\
    barrier	\
    bitmap	\
    brlock	\
    bytelock	\
    cohort	\
    epoch	\
    fifo	\
    hp		\
    hs		\
    rhs		\
    ht		\
    pflock	\
    pr		\
    queue	\
    ring	\
    rwlock	\
    swlock	\
    sequence	\
    spinlock	\
    stack	\
    swlock	\
    tflock

.PHONY: all clean check

all:
	$(MAKE) -C ./ck_array/validate all
	$(MAKE) -C ./ck_cohort/validate all
	$(MAKE) -C ./ck_cohort/benchmark all
	$(MAKE) -C ./ck_bitmap/validate all
	$(MAKE) -C ./ck_backoff/validate all
	$(MAKE) -C ./ck_queue/validate all
	$(MAKE) -C ./ck_brlock/validate all
	$(MAKE) -C ./ck_ht/validate all
	$(MAKE) -C ./ck_ht/benchmark all
	$(MAKE) -C ./ck_brlock/benchmark all
	$(MAKE) -C ./ck_spinlock/validate all
	$(MAKE) -C ./ck_spinlock/benchmark all
	$(MAKE) -C ./ck_fifo/validate all
	$(MAKE) -C ./ck_fifo/benchmark all
	$(MAKE) -C ./ck_pr/validate all
	$(MAKE) -C ./ck_pr/benchmark all
	$(MAKE) -C ./ck_hs/benchmark all
	$(MAKE) -C ./ck_hs/validate all
	$(MAKE) -C ./ck_rhs/benchmark all
	$(MAKE) -C ./ck_rhs/validate all
	$(MAKE) -C ./ck_barrier/validate all
	$(MAKE) -C ./ck_barrier/benchmark all
	$(MAKE) -C ./ck_bytelock/validate all
	$(MAKE) -C ./ck_bytelock/benchmark all
	$(MAKE) -C ./ck_epoch/validate all
	$(MAKE) -C ./ck_rwcohort/validate all
	$(MAKE) -C ./ck_rwcohort/benchmark all
	$(MAKE) -C ./ck_sequence/validate all
	$(MAKE) -C ./ck_sequence/benchmark all
	$(MAKE) -C ./ck_stack/validate all
	$(MAKE) -C ./ck_stack/benchmark all
	$(MAKE) -C ./ck_ring/validate all
	$(MAKE) -C ./ck_ring/benchmark all
	$(MAKE) -C ./ck_rwlock/validate all
	$(MAKE) -C ./ck_rwlock/benchmark all
	$(MAKE) -C ./ck_tflock/validate all
	$(MAKE) -C ./ck_tflock/benchmark all
	$(MAKE) -C ./ck_swlock/validate all
	$(MAKE) -C ./ck_swlock/benchmark all
	$(MAKE) -C ./ck_pflock/validate all
	$(MAKE) -C ./ck_pflock/benchmark all
	$(MAKE) -C ./ck_hp/validate all
	$(MAKE) -C ./ck_hp/benchmark all

clean:
	$(MAKE) -C ./ck_array/validate clean
	$(MAKE) -C ./ck_pflock/validate clean
	$(MAKE) -C ./ck_pflock/benchmark clean
	$(MAKE) -C ./ck_tflock/validate clean
	$(MAKE) -C ./ck_tflock/benchmark clean
	$(MAKE) -C ./ck_rwcohort/validate clean
	$(MAKE) -C ./ck_rwcohort/benchmark clean
	$(MAKE) -C ./ck_backoff/validate clean
	$(MAKE) -C ./ck_bitmap/validate clean
	$(MAKE) -C ./ck_queue/validate clean
	$(MAKE) -C ./ck_cohort/validate clean
	$(MAKE) -C ./ck_cohort/benchmark clean
	$(MAKE) -C ./ck_brlock/validate clean
	$(MAKE) -C ./ck_ht/validate clean
	$(MAKE) -C ./ck_ht/benchmark clean
	$(MAKE) -C ./ck_hs/validate clean
	$(MAKE) -C ./ck_hs/benchmark clean
	$(MAKE) -C ./ck_rhs/validate clean
	$(MAKE) -C ./ck_rhs/benchmark clean
	$(MAKE) -C ./ck_brlock/benchmark clean
	$(MAKE) -C ./ck_spinlock/validate clean
	$(MAKE) -C ./ck_spinlock/benchmark clean
	$(MAKE) -C ./ck_fifo/validate clean
	$(MAKE) -C ./ck_fifo/benchmark clean
	$(MAKE) -C ./ck_pr/validate clean
	$(MAKE) -C ./ck_pr/benchmark clean
	$(MAKE) -C ./ck_barrier/validate clean
	$(MAKE) -C ./ck_barrier/benchmark clean
	$(MAKE) -C ./ck_bytelock/validate clean
	$(MAKE) -C ./ck_bytelock/benchmark clean
	$(MAKE) -C ./ck_epoch/validate clean
	$(MAKE) -C ./ck_sequence/validate clean
	$(MAKE) -C ./ck_sequence/benchmark clean
	$(MAKE) -C ./ck_stack/validate clean
	$(MAKE) -C ./ck_stack/benchmark clean
	$(MAKE) -C ./ck_ring/validate clean
	$(MAKE) -C ./ck_ring/benchmark clean
	$(MAKE) -C ./ck_rwlock/validate clean
	$(MAKE) -C ./ck_rwlock/benchmark clean
	$(MAKE) -C ./ck_swlock/validate clean
	$(MAKE) -C ./ck_swlock/benchmark clean
	$(MAKE) -C ./ck_pflock/validate clean
	$(MAKE) -C ./ck_pflock/benchmark clean
	$(MAKE) -C ./ck_hp/validate clean
	$(MAKE) -C ./ck_hp/benchmark clean

check: all
	rc=0; 							\
	for d in $(DIR) ; do					\
		echo "----[ Testing $$d....";			\
		$(MAKE) -C ./ck_$$d/validate check || rc=1; 	\
		echo;						\
	done; 							\
	exit $$rc

