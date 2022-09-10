#!/bin/sh

mkdir -p ./bin

WARNINGS="-Wall -Wextra -Wpedantic -Wabi"

# This program uses my hashmap library available at: 
# https://github.com/z4221/hashmap
# tcc src/httpServer.c -o bin/httpServer
musl-clang $WARNINGS -march=native -static -O3 src/httpServer.c -o bin/httpServer
# clang -g $WARNINGS src/httpServer.c -o bin/httpServer
# clang $WARNINGS -O3 src/httpServer.c -o bin/httpServer

