# Qt6Macros.cmake - Qt-specific CMake helper macros

macro(add_qt_translations target ts_files)
    if(NOT TS_FILES)
        set(TS_FILES ${ts_files})
    endif()
    
    foreach(ts_file ${TS_FILES})
        get_filename_component(ts_name ${ts_file} NAME_WE)
        qt_add_translation(qm_file ${ts_file})
        list(APPENDqm_files ${qm_file})
    endforeach()
    
    qt_add_resources(${target} "translations" ${qm_files})
endmacro()

macro(add_qt_resources target resources_file)
    qt_add_resources(${target} ${resources_file})
endmacro()
