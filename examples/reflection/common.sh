target=${1%.cpp}
shift
prefix=$(< prefix)

clang_flags=(
	-cc1 -triple x86_64-pc-linux-gnu \
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
	-fdebug-compilation-dir $(dirname ${0}) \
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
	-resource-dir ${prefix}/lib/clang/8.0.0 \
	-internal-isystem ../../reflection \
	-internal-isystem /usr/include/c++/7 \
	-internal-isystem /usr/include/x86_64-linux-gnu/c++/7 \
	-internal-isystem ${prefix}/lib/clang/8.0.0/include \
	-internal-isystem /usr/local/include \
	-internal-externc-isystem /usr/include/x86_64-linux-gnu/ \
	-internal-externc-isystem /usr/include/ \
	-std=c++17 -xc++ \
	-o ${target}.o ${target}.cpp)
