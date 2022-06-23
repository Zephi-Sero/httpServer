#!/bin/sh

mkdir -p ./bin


# This program uses my hashmap library available at: 
# https://github.com/z4221/hashmap
# gcc -fsanitize=undefined -fsanitize=address -g src/httpServer.c -o bin/httpServer
# clang -g src/httpServer.c -o bin/httpServer
# musl-clang -static -O3 src/httpServer.c -o bin/httpServer
clang -O3  src/httpServer.c -o bin/httpServer
