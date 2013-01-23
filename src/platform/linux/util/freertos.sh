#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;freeRTOS.o, ;\
!freeRTOS.o, :\
\
c0.o-llboot.o;\
freeRTOS.o-\
" ./gen_client_stub
