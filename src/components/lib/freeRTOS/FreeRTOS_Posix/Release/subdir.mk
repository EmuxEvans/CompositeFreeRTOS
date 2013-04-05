################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../freertos_entry.c 

OBJS += \
./freertos_entry.o 

C_DEPS += \
./freertos_entry.d


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -DUSE_STDIO=1 -D__GCC_POSIX__=1 -I../Common_Demo/include -I.. -I../FreeRTOS_Kernel/include -I../FreeRTOS_Kernel/portable/GCC/Posix $(DIETINC) $(COS_COMP_INC) -O2 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


