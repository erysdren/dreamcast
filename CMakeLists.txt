cmake_minimum_required(VERSION 3.10)
project(dreamcast LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED TRUE)

#
# runtime
#

add_library(runtime INTERFACE)
target_sources(runtime INTERFACE
	${PROJECT_SOURCE_DIR}/runtime/start.s
	${PROJECT_SOURCE_DIR}/runtime/sh7091/c_serial.cpp
	${PROJECT_SOURCE_DIR}/runtime/sh7091/cache.cpp
	${PROJECT_SOURCE_DIR}/runtime/sh7091/serial.cpp
	${PROJECT_SOURCE_DIR}/runtime/holly/core/region_array.cpp
	${PROJECT_SOURCE_DIR}/runtime/runtime.cpp
	${PROJECT_SOURCE_DIR}/runtime/printf/unparse.c
	${PROJECT_SOURCE_DIR}/runtime/printf/printf.c
	${PROJECT_SOURCE_DIR}/runtime/printf/parse.c
	${PROJECT_SOURCE_DIR}/runtime/tinyalloc/tinyalloc.c
	${PROJECT_SOURCE_DIR}/runtime/ibsp.c
	${PROJECT_SOURCE_DIR}/runtime/ibsp_trace.c
	${PROJECT_SOURCE_DIR}/runtime/pvr.c
	${PROJECT_SOURCE_DIR}/runtime/utils.c
	${PROJECT_SOURCE_DIR}/runtime/camera.c
	${PROJECT_SOURCE_DIR}/runtime/texture_cache.cpp
	${PROJECT_SOURCE_DIR}/runtime/transfer.cpp
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
	$<$<COMPILE_LANGUAGE:C,CXX>:-MMD>
	$<$<COMPILE_LANGUAGE:C,CXX>:-MP>
	$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
	$<$<COMPILE_LANGUAGE:CXX>:-fno-non-call-exceptions>
	$<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
	$<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
	$<$<COMPILE_LANGUAGE:ASM>:--isa=sh4>
	$<$<COMPILE_LANGUAGE:ASM>:--little>
)
target_include_directories(runtime INTERFACE ${PROJECT_SOURCE_DIR}/runtime/)
target_link_directories(runtime INTERFACE ${PROJECT_SOURCE_DIR}/runtime/linker/)
target_link_libraries(runtime INTERFACE gcc)
target_link_options(runtime INTERFACE
	-Wl,-Tmain.lds
	-Wl,--gc-sections
	-Wl,--no-warn-rwx-segment
	-Wl,--entry=_start
)

function(dc_copy_binary target)
	add_custom_command(
		TARGET ${target}
		POST_BUILD
		COMMAND ${CMAKE_OBJCOPY} ARGS -O binary $<TARGET_FILE:${target}> ${PROJECT_BINARY_DIR}/$<TARGET_NAME:${target}>.bin
	)
endfunction()

function(dc_make_cdi target)
	add_custom_command(
		TARGET ${target}
		POST_BUILD
		COMMAND mkdcdisc ARGS -e $<TARGET_FILE:${target}> -o ${PROJECT_BINARY_DIR}/$<TARGET_NAME:${target}>.cdi -N
	)
endfunction()

function(dc_make_zip target destination)
	add_custom_command(
		TARGET ${target}
		PRE_BUILD
		WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/content
		COMMAND 7z ARGS a -m0=Copy ${destination} maps models textures
	)
endfunction()

function(dc_make_romfs target source destination)
	add_custom_command(
		TARGET ${target}
		PRE_BUILD
		WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/apps
		COMMAND zip2rom ARGS ${source} ${destination}
	)
endfunction()

function(dc_add_app name)
	cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "SOURCES")
	add_executable(${name} ${ARG_SOURCES})
	target_link_libraries(${name} PUBLIC runtime)
	dc_copy_binary(${name})
	dc_make_cdi(${name})
endfunction()

#
# apps
#

set(single_file_demos cube_ta cube_ta_fullscreen cube_ta_fullscreen_textured maple maple2)

foreach(demo IN LISTS single_file_demos)
	# dc_add_app(${demo} SOURCES apps/${demo}.c)
endforeach()

# dc_add_app(cube_ta_fullscreen_textured2 SOURCES apps/cube_ta_fullscreen_textured.cpp)
# dc_add_app(cube_ta_fullscreen_textured_punch_through SOURCES apps/cube_ta_fullscreen_textured_punch_through.cpp)
# dc_add_app(font SOURCES apps/font.cpp)
# dc_add_app(pvr_test1 SOURCES apps/pvr_test1.cpp)
# dc_make_romfs(pvr_test1 dc_map01_minimal.pk3 dc_map01_minimal.pk3.h)
# dc_add_app(dreamcasko SOURCES apps/dreamcasko.cpp)
# dc_make_romfs(dreamcasko dreamcasko.pk3 dreamcasko.pk3.h)
# dc_add_app(dc_map01_minimal SOURCES apps/dc_map01_minimal.cpp)
dc_add_app(trade-federation-ship SOURCES apps/trade-federation-ship.cpp)
# dc_add_app(rott SOURCES apps/rott.cpp)
