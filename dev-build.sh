#!/bin/bash
source base.sh
cd_into_build
ln -s ../shaders/* .
compile
run_on_success ./livecode $@
