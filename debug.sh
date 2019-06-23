#!/bin/bash
source ./base.sh
cd_into_build

cgdb livecode \
    -ex "set args lua/mouse.lua" \
    -ex "run" 
