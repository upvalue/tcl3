#!/bin/bash

echo "Building projects..."

# Build cpp
cd cpp
if make; then
    echo "[CPP] SUCCESS"
    cpp_status="SUCCESS"
else
    echo "[CPP] FAIL"
    cpp_status="FAIL"
fi

# Build picol
cd ../picol
if make; then
    echo "[PICOL] SUCCESS"
    picol_status="SUCCESS"
else
    echo "[PICOL] FAIL"
    picol_status="FAIL"
fi

cd ..

echo "Build summary: CPP=$cpp_status PICOL=$picol_status"