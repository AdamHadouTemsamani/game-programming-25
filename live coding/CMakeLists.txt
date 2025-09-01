file(GLOB file_src_list "*.c" "*.cpp")
foreach(file_src ${file_src_list})
	get_filename_component(targetname ${file_src} NAME)
	add_executable(${targetname} ${file_src})
