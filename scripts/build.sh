#!/bin/bash

target_release() {
    cd release
    cmake -DCMAKE_BUILD_TYPE=Release ../..
    make
    echo "Built target in build/release/"
    cd ../..
}

target_debug() {
    cd debug 
    cmake -DCMAKE_BUILD_TYPE=Debug ../..
    make
    echo "Built target in build/debug/"
    cd ../..
}

# Create folder for distribution
if [ "$1" = "release" ]
then
    if [ -d "$box2d-example" ]
    then
        rm -rf -d box2d-example
    fi

    mkdir -p box2d-example
fi

# Creates the folder for the buildaries
mkdir -p box2d-example 
mkdir -p box2d-example/assets
mkdir -p build
mkdir -p build/release
mkdir -p build/debug
cd build

# Builds target
if [ "$1" = "release" ]
then
    target_release
    cp build/release/box2d-example box2d-example/box2d-example
else
    target_debug
fi

cp -R assets box2d-example/
