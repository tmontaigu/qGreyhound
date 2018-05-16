cmake_minimum_required(VERSION 3.0)


option( INSTALL_QGREYHOUND_PLUGIN "Check to install qGreyhound plugin" OFF )

# CloudCompare 'DUMMY' plugin
if (INSTALL_QGREYHOUND_PLUGIN)
    project( QGREYHOUND_PLUGIN )



    #load necessary libraries (see qPCV for an example)
    #add_subdirectory (LIB1)
    
    #if the plugin is an 'OpenGL filter', uncomment the line below
    #set( CC_OPENGL_FILTER ON BOOL)
    include( ../../CMakePluginTpl.cmake )
    
    #set dependencies to necessary libraries (see qPCV for an example)
    #target_link_libraries( ${PROJECT_NAME} LIB1 )
    #include_directories( ${LIB1_INCLUDE_DIR} )

    find_package(Qt5Network REQUIRED)
	target_link_libraries(QGREYHOUND_PLUGIN Qt5::Network)



	find_package(PDAL 1.0.0 REQUIRED CONFIG)
	if ( NOT PDAL_FOUND )
		message( SEND_ERROR "PDAL package not found!")
	else()
		include_directories(${PDAL_INCLUDE_DIRS})
		link_directories(${PDAL_LIBRARY_DIRS})
	endif()

	if( PDAL_FOUND )
		add_definitions(${PDAL_DEFINITIONS})
		target_link_libraries(QGREYHOUND_PLUGIN ${PDAL_LIBRARIES})

		if( WIN32 )
			if ( MSVC_VERSION GREATER_EQUAL 1900 ) # Visual Studio 2017
				add_definitions(-DWIN32_LEAN_AND_MEAN)
			endif()

			target_link_libraries(QGREYHOUND_PLUGIN "pdal_util")
		endif()
	else()
		message( SEND_ERROR "PDAL package not found: can't link" )
	endif()

	include_directories( ${PDAL_GREYHOUND_INCLUDE} )
	include_directories( ${ARBITER_INCLUDE} ) 

	find_library(PDAL_GREYHOUND_READER libpdal_plugin_reader_greyhound HINTS ${PDAL_LIBRARY_DIRS})
	target_link_libraries(QGREYHOUND_PLUGIN  ${PDAL_GREYHOUND_READER})

	find_library(ARBITER arbiter HINTS ${ARBITER_LIB})
	target_link_libraries(QGREYHOUND_PLUGIN ${ARBITER})



endif()
