#!/usr/bin/bash

cc -std=c11 cun.c -o cun
if [ $? -eq 0 ]; then
    ./cun "$@"
fi
