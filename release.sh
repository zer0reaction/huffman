#!/bin/bash

set -xe

gcc -O2 -std=c89 -s -o ./hz ./main.c
