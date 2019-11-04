# VisFuzz

## Overview
VisFuzz is an interactive tool for better understanding and intervening fuzzing process via real-time visualization. VisFuzz helps the test engineer to: 1) drill down into the bottleneck from function level, basic block level to statement level; 2) learn semantic context from basic blocks and source code; 3) construct targeted inputs or update the test driver to increase coverage.
## Published work
VisFuzz: Understanding and Intervening Fuzzing with Interactive Visualization, ASE 2019.

[preprint paper](http://www.wingtecher.com/themes/WingTecherResearch/assets/papers/visfuzzASE19r.pdf)

[demo video link](https://youtu.be/opjRKcqOvNs)

## Usage
1. Install LLVM (>= 7.0), python3.
2. Download and compile VisFuzz:

        git clone https://github.com/ChijinZ/VisFuzz.git
        export DEMO_PATH=$PWD/demo
        export TOOL_PATH=$PWD/visfuzz
        cd $TOOL_PATH/fuzz
        mkdir build
        cd build
        cmake ../llvm/ .
        export VISFUZZ_BUILD=$PWD
        cd $TOOL_PATH/fuzz/afl
        make

3. Fuzz && visualize demo:
 
        cd $DEMO_PATH
        sh compile.sh
        cd repo
        nohup $TOOL_PATH/fuzz/afl/afl-fuzz -i in -o out ./app @@ &
        python $TOOL_PATH/open_file_server.py 6767

        # Open a new terminal
        cd $TOOL_PATH/visualization
        python -m http.server 8000

4. Open browser (*Chrome* is recommended) and visit localhost:8000
