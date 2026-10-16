#!/bin/bash
stty erase '^H'
export LD_PRELOAD=./libsandbox_alloc.so
exec "$@"
