#!/bin/bash
source ./base.sh
cd_into_build

coredumpctl dump -o dump
cgdb livecode dump
