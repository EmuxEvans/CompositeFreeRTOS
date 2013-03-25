C_OBJS=freertos.o
ASM_OBJS=cos_asm_scheduler.o
COMPONENT=freeRTOS.o
INTERFACES=sched_hier
DEPENDENCIES=sched_hier checkpoint
FN_PREPEND=parent_
IF_LIB=./freertos_posix.o
FREERTOSOBJ=../../../lib/freeRTOS/FreeRTOS_Posix/Release/freertos_posix.o

pre: first all
first:
	$(info |     [MAKE] compiling freeRTOS lib...)
	make -C ../../../lib/freeRTOS/FreeRTOS_Posix/Release clean
	make -C ../../../lib/freeRTOS/FreeRTOS_Posix/Release
	$(info |     [CP]   copying freertos_posix.o from freeRTOS)
	@cp $(FREERTOSOBJ) .

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
 MANDITORY_LIB=simple_stklib.o
endif

