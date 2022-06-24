#!/bin/bash -eu
J8=-j8
CLANGPP=clang++-10
LLVMLINK=llvm-link-10
OPT=opt-10
REPO=https://github.com/nlohmann/json.git
REVISION=b04543ecc58188a593f8729db38c2c87abd90dc3

CXXFLAGS='-g -flto'

[[ -d repo ]] || git clone "$REPO" repo
mkdir -p repo/bitcode

cd repo
git checkout -f "$REVISION"

cp test/src/fuzzer-parse_json.cpp ./target.cc
"$CLANGPP" $CXXFLAGS target.cc  -I src -c -o bitcode/target.o

cd bitcode
"$LLVMLINK" *.o -o combined.bc
"$OPT" -load=$VISFUZZ_BUILD/libVisFuzz.so -visfuzz -fvisfuzz-export=static.json \
       combined.bc -o instru.bc
"$CLANGPP" -g -fuse-ld=lld instru.bc $VISFUZZ_BUILD/libVisFuzzDriver.a -pthread -o app

cp ./{app,static.json} ../
