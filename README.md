Boost gauge with diagnostics groups logger (to SD card).

Tested on VAG 1.8T AUM/AUQ. Should work on other 1.8T engines with wideband lambda probe (ECU ME7.5).

Uses KW1281 diagnostics protocol. It can also connect to Cluster and show oil temperature.

LCD display type CGG128064AY00-FHW with UC1601 controller (can be easily adopted to any other SPI controlled 128x64 B/W display).

Microcontroller: STM32F103CB

Uses FreeRTOS and FatFS filesystem