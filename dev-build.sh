#!/bin/bash
source base.sh
cd_into_build
compile
run_on_success ./livecode ../shaders/vertex/one_quad.vertex_shader ../shaders/fragment/scene.fragment_shader
