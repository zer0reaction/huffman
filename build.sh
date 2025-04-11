#!/bin/bash

set -xe

gcc -o sizecheck ../util/sizecheck.c
./sizecheck
gcc -Wall -Wextra -pedantic -fsanitize=address -std=c89 -s -o ./huffman ./main.c
