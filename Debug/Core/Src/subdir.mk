################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/buck.c \
../Core/Src/coil_ctrl.c \
../Core/Src/current_sensor.c \
../Core/Src/hbridge.c \
../Core/Src/i2c_app.c \
../Core/Src/main.c \
../Core/Src/rs485.c \
../Core/Src/safety.c \
../Core/Src/sensor_report.c \
../Core/Src/stm32h7xx_hal_msp.c \
../Core/Src/stm32h7xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32h7xx.c \
../Core/Src/uart_app.c 

OBJS += \
./Core/Src/buck.o \
./Core/Src/coil_ctrl.o \
./Core/Src/current_sensor.o \
./Core/Src/hbridge.o \
./Core/Src/i2c_app.o \
./Core/Src/main.o \
./Core/Src/rs485.o \
./Core/Src/safety.o \
./Core/Src/sensor_report.o \
./Core/Src/stm32h7xx_hal_msp.o \
./Core/Src/stm32h7xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32h7xx.o \
./Core/Src/uart_app.o 

C_DEPS += \
./Core/Src/buck.d \
./Core/Src/coil_ctrl.d \
./Core/Src/current_sensor.d \
./Core/Src/hbridge.d \
./Core/Src/i2c_app.d \
./Core/Src/main.d \
./Core/Src/rs485.d \
./Core/Src/safety.d \
./Core/Src/sensor_report.d \
./Core/Src/stm32h7xx_hal_msp.d \
./Core/Src/stm32h7xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32h7xx.d \
./Core/Src/uart_app.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_PWR_LDO_SUPPLY -DUSE_HAL_DRIVER -DSTM32H753xx -c -I../Core/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/buck.cyclo ./Core/Src/buck.d ./Core/Src/buck.o ./Core/Src/buck.su ./Core/Src/coil_ctrl.cyclo ./Core/Src/coil_ctrl.d ./Core/Src/coil_ctrl.o ./Core/Src/coil_ctrl.su ./Core/Src/current_sensor.cyclo ./Core/Src/current_sensor.d ./Core/Src/current_sensor.o ./Core/Src/current_sensor.su ./Core/Src/hbridge.cyclo ./Core/Src/hbridge.d ./Core/Src/hbridge.o ./Core/Src/hbridge.su ./Core/Src/i2c_app.cyclo ./Core/Src/i2c_app.d ./Core/Src/i2c_app.o ./Core/Src/i2c_app.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/rs485.cyclo ./Core/Src/rs485.d ./Core/Src/rs485.o ./Core/Src/rs485.su ./Core/Src/safety.cyclo ./Core/Src/safety.d ./Core/Src/safety.o ./Core/Src/safety.su ./Core/Src/sensor_report.cyclo ./Core/Src/sensor_report.d ./Core/Src/sensor_report.o ./Core/Src/sensor_report.su ./Core/Src/stm32h7xx_hal_msp.cyclo ./Core/Src/stm32h7xx_hal_msp.d ./Core/Src/stm32h7xx_hal_msp.o ./Core/Src/stm32h7xx_hal_msp.su ./Core/Src/stm32h7xx_it.cyclo ./Core/Src/stm32h7xx_it.d ./Core/Src/stm32h7xx_it.o ./Core/Src/stm32h7xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32h7xx.cyclo ./Core/Src/system_stm32h7xx.d ./Core/Src/system_stm32h7xx.o ./Core/Src/system_stm32h7xx.su ./Core/Src/uart_app.cyclo ./Core/Src/uart_app.d ./Core/Src/uart_app.o ./Core/Src/uart_app.su

.PHONY: clean-Core-2f-Src

