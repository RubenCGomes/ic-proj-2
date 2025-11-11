#!/usr/bin/bash

echo "================================================"
echo "Golomb Coding - Command Line Tool Demo"
echo "================================================"
echo ""

# Check if binary exists
if [ ! -f bin/golomb_main ]; then
    echo "Error: bin/golomb_main not found. Please compile first."
    exit 1
fi

echo "1. Basic Encoding - Single Value"
echo "Command: ./bin/golomb_main encode 5"
echo "----------------------------------------"
./bin/golomb_main encode 5
echo ""
echo ""

echo "2. Encoding Multiple Values (with negative numbers)"
echo "Command: ./bin/golomb_main encode 0 1 -1 2 -2 3 -3"
echo "----------------------------------------"
./bin/golomb_main encode 0 1 -1 2 -2 3 -3
echo ""
echo ""

echo "3. Encoding with Custom m Parameter"
echo "Command: ./bin/golomb_main -m 8 encode 25 50 75"
echo "----------------------------------------"
./bin/golomb_main -m 8 encode 25 50 75
echo ""
echo ""

echo "4. Encoding with Sign-Magnitude Mode"
echo "Command: ./bin/golomb_main -mode sign-magnitude encode 10 -10"
echo "----------------------------------------"
./bin/golomb_main -mode sign-magnitude encode 10 -10
echo ""
echo ""

echo "5. Decoding Bit Strings"
echo "Command: ./bin/golomb_main decode 100 111 110"
echo "----------------------------------------"
./bin/golomb_main decode 100 111 110
echo ""
echo ""

echo "6. Round-Trip Verification"
echo "Encoding value 42 then decoding it back..."
echo "----------------------------------------"
echo "Step 1 - Encode:"
./bin/golomb_main -m 8 encode 42 | grep "42 ->"
echo ""
echo "Step 2 - Decode:"
./bin/golomb_main -m 8 decode 00000000001100
echo ""
echo ""

echo "7. Large Numbers with Appropriate m"
echo "Command: ./bin/golomb_main -m 16 encode 100 200 300"
echo "----------------------------------------"
./bin/golomb_main -m 16 encode 100 200 300
echo ""
echo ""
