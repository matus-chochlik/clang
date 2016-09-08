#!/bin/bash
source $(dirname ${0})/common.sh

${prefix}/bin/clang++ \
	-std=c++14 \
	-isystem ../../reflection \
	-Xclang -freflection \
	-o $(dirname $0)/${target} \
	$(dirname $0)/${target}.cpp && $(dirname $0)/${target}
