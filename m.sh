#!/bin/sh
gcc -std=c99 cnc_gen.c -lm -o cnc_gen
gcc -std=c99 weld.c -lm -o weld
