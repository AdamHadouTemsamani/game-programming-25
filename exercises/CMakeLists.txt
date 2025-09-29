file(GLOB file_src_list "*.c" "*.cpp")
file(GLOB imgui_src_list "${CMAKE_SOURCE_DIR}/lib/imgui/imgui/*.cpp")

foreach(file_src ${file_src_list})
	get_filename_component(targetname ${file_src} NAME)
	add_executable(${targetname} ${file_src} ${imgui_src_list})
	
	target_include_directories(${targetname} PRIVATE ${CMAKE_SOURCE_DIR}/lib/itu)
	target_include_directories(${targetname} PRIVATE ${CMAKE_SOURCE_DIR}/lib/imgui)

	target_link_libraries(${targetname} PRIVATE SDL3::SDL3)
	target_link_libraries(${targetname} PRIVATE SDL3_mixer::SDL3_mixer)
	target_link_libraries(${targetname} PRIVATE box2d::box2d)
<<<<<<< HEAD
	# target_link_libraries(${targetname} PRIVATE imgui)
endforeach()
=======
	target_link_libraries(${targetname} PRIVATE imgui)
endforeach()
>>>>>>> upstream/main
