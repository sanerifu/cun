#!/usr/bin/bash

cc -std=c11 cun.c -o cun -fsanitize=address
if [ $? -eq 0 ]; then
    ./cun "$@"
fi
