#!/bin/bash
source $(dirname ${0})/common.sh

${prefix}/bin/clang++ \
	-isystem ../../reflection \
	-std=c++14 \
	-o ${target} \
	${target}.cpp && ./${target}
