file(GLOB file_src_list "*.c" "*.cpp")

foreach(file_src ${file_src_list})
	get_filename_component(targetname ${file_src} NAME)
	add_executable(${targetname} ${file_src})
	
	target_include_directories(${targetname} PRIVATE ${CMAKE_SOURCE_DIR}/lib/itu)
	target_include_directories(${targetname} PRIVATE ${CMAKE_SOURCE_DIR}/lib/imgui)

	target_link_libraries(${targetname} PRIVATE SDL3::SDL3)
	target_link_libraries(${targetname} PRIVATE SDL3_mixer::SDL3_mixer)
	target_link_libraries(${targetname} PRIVATE box2d::box2d)
	target_link_libraries(${targetname} PRIVATE imgui)
endforeach()
