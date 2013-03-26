#!/bin/sh

./cos_loader \
"c0.o, ;crboot.o, ;freeRTOS.o, :\
\
c0.o-crboot.o;\
freeRTOS.o-[parent_]crboot.o\
" ./gen_client_stub
