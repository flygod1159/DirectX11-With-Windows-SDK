cmake_minimum_required (VERSION 3.12)

add_subdirectory("36 Deferred Shading Beginning")

if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Texture)
	file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/Texture DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()

if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Model)
	file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/Model DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()
