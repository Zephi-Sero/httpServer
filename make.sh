#!/bin/sh

mkdir -p ./bin

WARNINGS="-Wall -Wextra -Wpedantic -Wabi"

tcc src/http-server.c -o bin/httpServer
# musl-clang $WARNINGS -march=native -static -O3 src/http-server.c -o bin/httpServer
# gcc -g $WARNINGS src/http-server.c -o bin/httpServer
# clang -g $WARNINGS src/http-server.c -o bin/httpServer
# clang $WARNINGS -O3 src/http-server.c -o bin/httpServer

