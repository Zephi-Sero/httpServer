#!/bin/sh

mkdir -p ./bin

musl-gcc -static src/httpServer.c -o bin/httpServer
