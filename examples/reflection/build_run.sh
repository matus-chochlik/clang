#!/bin/bash
source $(dirname ${0})/common.sh

${prefix}/bin/clang++ \
	-isystem ../../reflection \
	-Xclang -freflection \
	-std=c++17 \
	-o ${target} \
	${target}.cpp && ./${target}
