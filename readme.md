# VisFuzz

## Overview

## Usage
1. Install LLVM (>= 7.0), python3.
2. Download and compile VisFuzz:

    git clone https://github.com/ChijinZ/VisFuzz.git
    export TOOL_PATH=$PWD/VisFuzz
    cd $TOOL_PATH/visfuzz/fuzz
    mkdir build
    cd build
    cmake ../llvm/ .

    cd $TOOL_PATH/visfuzz/afl
    make

3. Fuzz && visualize demo: