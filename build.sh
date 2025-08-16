#!/bin/bash

echo "Building projects..."

# Build cpp
cd cpp
if make; then
    echo "[CPP] SUCCESS"
    cpp_status="SUCCESS"
    echo "c++ interpreter available at ./cpp/repl"
else
    echo "[CPP] FAIL"
    cpp_status="FAIL"
fi

# Build picol
cd ../picol
if make; then
    echo "[PICOL] SUCCESS"
    picol_status="SUCCESS"
    echo "picol available at ./picol/picol"
else
    echo "[PICOL] FAIL"
    picol_status="FAIL"
fi

cd ../zig
if zig build --release=fast; then
    echo "[ZIG] SUCCESS"
    zig_status="SUCCESS"
else
    echo "[ZIG] FAIL"
    zig_status="FAIL"
    echo "zig interpreter available at ./zig/zig-out/bin/tcl"
fi

cd ../rust
if cargo build --release; then
    echo "[RUST] SUCCESS"
    rust_status="SUCCESS"
else
    echo "[RUST] FAIL"
    rust_status="FAIL"
    echo "rust interpreter available at ./rust/target/release/tcl"

cd ..
echo "Build summary: CPP=$cpp_status PICOL=$picol_status ZIG=$zig_status"
