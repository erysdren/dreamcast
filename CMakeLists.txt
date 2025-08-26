cmake_minimum_required(VERSION 3.10)
project(dreamcast LANGUAGES C CXX ASM)

#
# runtime
#

add_library(runtime INTERFACE)
target_sources(runtime INTERFACE
	${PROJECT_SOURCE_DIR}/runtime/start.s
	${PROJECT_SOURCE_DIR}/runtime/sh7091/cache.cpp
	${PROJECT_SOURCE_DIR}/runtime/runtime.cpp
)
target_compile_options(runtime INTERFACE
	$<$<COMPILE_LANGUAGE:C,CXX>:-mfsrra>
	$<$<COMPILE_LANGUAGE:C,CXX>:-mfsca>
	$<$<COMPILE_LANGUAGE:C,CXX>:-ffast-math>
	$<$<COMPILE_LANGUAGE:C,CXX>:-m4-single-only>
	$<$<COMPILE_LANGUAGE:C,CXX>:-ml>
	$<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>
	$<$<COMPILE_LANGUAGE:C,CXX>:-nostdlib>
	$<$<COMPILE_LANGUAGE:C,CXX>:-Og>
	$<$<COMPILE_LANGUAGE:C,CXX>:-ffunction-sections>
	$<$<COMPILE_LANGUAGE:C,CXX>:-fdata-sections>
	$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
	$<$<COMPILE_LANGUAGE:CXX>:-fno-non-call-exceptions>
	$<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
	$<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
	$<$<COMPILE_LANGUAGE:ASM>:--isa=sh4>
	$<$<COMPILE_LANGUAGE:ASM>:--little>
)
target_include_directories(runtime INTERFACE ${PROJECT_SOURCE_DIR}/runtime/)
target_include_directories(runtime INTERFACE ${PROJECT_SOURCE_DIR}/runtime/sh7091/)
target_link_directories(runtime INTERFACE ${PROJECT_SOURCE_DIR}/runtime/linker/)
target_link_options(runtime INTERFACE
	-Wl,-Tmain.lds
	-Wl,--gc-sections
	-Wl,--no-warn-rwx-segment
	-Wl,--entry=_start
)

function(copy_binary target)
	add_custom_command(
		TARGET ${target}
		POST_BUILD
		COMMAND ${CMAKE_OBJCOPY} ARGS -O binary $<TARGET_FILE:${target}> ${PROJECT_BINARY_DIR}/${target}.bin
	)
endfunction()

#
# apps
#

set(single_file_demos cube_ta cube_ta_fullscreen cube_ta_fullscreen_textured maple)

foreach(demo IN LISTS single_file_demos)
	add_executable(${demo} apps/${demo}.c)
	target_link_libraries(${demo} PUBLIC runtime)
	copy_binary(${demo})
endforeach()
