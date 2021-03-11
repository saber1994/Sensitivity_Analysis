add_library(
FaultInjection MODULE
# List your source files here.
FaultInjection.cpp
PreProcess.cpp
)

# Use C++11 to compile your pass (i.e., supply -std=c++11).
target_compile_features(
FaultInjection PRIVATE cxx_range_for cxx_auto_type

)

# LLVM is (typically) built with no C++ RTTI. We need to match that;
# otherwise, we'll get linker errors about missing RTTI data.
set_target_properties(
FaultInjection PROPERTIES
COMPILE_FLAGS "-fPIC -fno-rtti"
)

