file(GLOB OPEN303_SOURCES CONFIGURE_DEPENDS dsp/*.cpp)
add_library(open303 STATIC engine.cpp ${OPEN303_SOURCES})
target_include_directories(open303 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(open303 PUBLIC cxx_std_20)
# IEEE checks and the legacy DSP's pointer aliasing must remain valid.
target_compile_options(open303 PRIVATE -fno-fast-math -fno-unsafe-math-optimizations -fno-strict-aliasing -fno-lto)
set_target_properties(open303 PROPERTIES UNITY_BUILD OFF)
