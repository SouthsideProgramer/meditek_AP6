#!/bin/sh
# Cross-compile for Meditek board
# Adjust the cross-compiler prefix to match your SDK

CROSS=${CROSS:-arm-linux-gnueabi-}

echo "Using cross-compiler: ${CROSS}gcc"

make CROSS=$CROSS clean cross

echo ""
echo "=== Built files ==="
ls -lh tcp_server tcp_client udp_server udp_client
file tcp_server
