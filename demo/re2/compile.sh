#!/bin/bash -eu
J8=-j8
CLANGPP=clang++-10
LLVMLINK=llvm-link-10
OPT=opt-10
CXXFLAGS='-g -flto'

[[ -d repo ]] || git clone https://github.com/google/re2.git repo
mkdir -p repo/bitcode
cd repo
make clean
cp ../target.cc ./
git checkout -f 499ef7eff7455ce9c9fae86111d4a77b6ac335de
CXX="$CLANGPP" CXXFLAGS="$CXXFLAGS" make "$J8" obj/libre2.a
"$CLANGPP" $CXXFLAGS ./target.cc -c -o bitcode/target.o -I .
cd bitcode
ar x ../obj/libre2.a
"$LLVMLINK" *.o -o combined.bc
"$OPT" -load=$VISFUZZ_BUILD/libVisFuzz.so -visfuzz -fvisfuzz-export=static.json \
       combined.bc -o instru.bc
"$CLANGPP" -g -fuse-ld=lld instru.bc $VISFUZZ_BUILD/libVisFuzzDriver.a -pthread -o app
cp {app,static.json} ..

