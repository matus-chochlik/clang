#!/bin/bash
source $(dirname ${0})/common.sh

${prefix}/bin/clang \
	-cc1 -triple x86_64-unknown-linux-gnu \
	-emit-obj -mrelax-all -disable-free \
	-main-file-name ${target}.cpp \
	-mrelocation-model static \
	-mthread-model posix \
	-mdisable-fp-elim \
	-masm-verbose \
	-mconstructor-aliases \
	-munwind-tables \
	-fmath-errno \
	-fuse-init-array \
	-fdeprecated-macro \
	-fdebug-compilation-dir $(dirname $0) \
	-ferror-limit 30 \
	-fmessage-length 120 \
	-fobjc-runtime=gcc \
	-fcxx-exceptions \
	-fexceptions \
	-fdiagnostics-show-option \
	-fcolor-diagnostics \
	-freflection \
	-target-cpu x86-64 \
	-dwarf-column-info \
	-debugger-tuning=gdb \
	-resource-dir ${prefix}/lib/clang/3.9.0 \
	-internal-isystem ../../reflection \
	-internal-isystem /usr/include/c++/4.8 \
	-internal-isystem /usr/include/x86_64-linux-gnu/c++/4.8 \
	-internal-isystem /usr/include/c++/4.8/backward \
	-internal-isystem /usr/local/include \
	-internal-isystem ${prefix}/lib/clang/3.9.0/include \
	-internal-externc-isystem /usr/include/x86_64-linux-gnu/ \
	-internal-externc-isystem /include/ \
	-internal-externc-isystem /usr/include/ \
	-std=c++14 -x c++ \
	-ast-dump ${target}.cpp
