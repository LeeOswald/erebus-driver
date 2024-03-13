

add_library(${EREBUS_DRIVERLIB} SHARED
    ../include/erebus/erebuskm.hxx
    erebuskm.cxx
)


add_compile_definitions(EREBUSKM_EXPORTS=1)

target_compile_features(${EREBUS_DRIVERLIB} PUBLIC ${ER_CXX_FEATURES})

target_link_libraries(${EREBUS_DRIVERLIB} PRIVATE ${EREBUS_RTLLIB})
