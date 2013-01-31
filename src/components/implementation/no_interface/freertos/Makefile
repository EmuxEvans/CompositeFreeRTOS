C_OBJS=freertos.o
ASM_OBJS=cos_asm_scheduler.o
COMPONENT=freeRTOS.o
INTERFACES=
DEPENDENCIES=
IF_LIB=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
 MANDITORY_LIB=simple_stklib.o
endif
