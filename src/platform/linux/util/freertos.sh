#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;freeRTOS.o, :\
\
c0.o-llboot.o;\
freeRTOS.o-[parent_]llboot.o\
" ./gen_client_stub
