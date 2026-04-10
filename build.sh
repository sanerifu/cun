#!/usr/bin/bash

cc -std=c11 cun.c -o cun -fsanitize=address -g3 -lreadline
if [ $? -eq 0 ]; then
    ./cun "$@"
fi
