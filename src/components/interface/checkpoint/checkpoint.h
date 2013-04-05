#ifndef   	CHECKPOINT_H
#define   	CHECKPOINT_H

int checkpoint_checkpt(spdid_t caller);
int checkpoint_restore(spdid_t caller);
int checkpoint_test_should_fault_again();

#endif 	    /* !CHECKPOINT_H */
