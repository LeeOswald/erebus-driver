set(SOURCE_FILES
    
    main.cpp
    
)

add_executable(${EREBUS_KMTEST} ${SOURCE_FILES})

target_compile_features(${EREBUS_KMTEST} PUBLIC ${EREBUS_CXX_FEATURES})

# Qt6Widgets_LIBRARIES variable also includes QtGui and QtCore
target_link_libraries (${EREBUS_KMTEST} PRIVATE ${EREBUS_DRIVERLIB} ${EREBUS_RTLLIB})
