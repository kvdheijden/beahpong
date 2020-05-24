set(ARCH arm)
set(CROSS_COMPILE /opt/raspberrypi/tools/arm-bcm2708/arm-linux-gnueabihf/bin/arm-linux-gnueabihf-)
set(KDIR /opt/raspberrypi/linux)

set(KERNEL_HEADERS ${KDIR}/include ${KDIR}/arch/${ARCH}/include)

set(KERNEL_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/beahpong.ko")
set(DEVICE_TREE_BLOB_OVERLAY "${CMAKE_CURRENT_BINARY_DIR}/beahpong.dtbo")

include_directories(${KERNEL_HEADERS})
add_definitions(-D__KERNEL__ -DMODULE)

set(LKM_SOURCES beahpong.c)

add_library(_dummy_lkm ${LKM_SOURCES})
target_include_directories(_dummy_lkm SYSTEM INTERFACE ${KDIR}/include ${KDIR}/arch/${ARCH}/include)
add_definitions(-D__KERNEL__ -DMODULE)

add_custom_command(
        OUTPUT ${KERNEL_OBJECT}
        COMMAND $(MAKE) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} -C ${KDIR} M=${CMAKE_CURRENT_BINARY_DIR} src=${CMAKE_CURRENT_SOURCE_DIR} modules
        DEPENDS ${LKM_SOURCES}
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        VERBATIM
)

add_custom_command(
        OUTPUT ${DEVICE_TREE_BLOB_OVERLAY}
        COMMAND ${CMAKE_CXX_COMPILER} -E -Wp,-MD,${CMAKE_CURRENT_BINARY_DIR}/beahpong.pre.tmp -nostdinc -I${KDIR}/arch/${ARCH}/boot/dts -I${KDIR}/include -undef -D__DTS__ -x assembler-with-cpp -o ${CMAKE_CURRENT_BINARY_DIR}/beahpong.tmp ${CMAKE_CURRENT_SOURCE_DIR}/beahpong.dts
        COMMAND ${KDIR}/scripts/dtc/dtc -@ -I dts -O dtb -o beahpong.dtbo ${CMAKE_CURRENT_BINARY_DIR}/beahpong.tmp
        MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/beahpong.dts"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        VERBATIM
)

add_custom_target(
        lkm ALL
        DEPENDS ${KERNEL_OBJECT}
)

add_custom_target(
        dts ALL
        DEPENDS ${DEVICE_TREE_BLOB_OVERLAY}
)
