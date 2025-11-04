#!/bin/bash

if [ "$1" = "release" ]
then
    ./build/release/box2d-example
else
    ./build/debug/box2d-example
fi