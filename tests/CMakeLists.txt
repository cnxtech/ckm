SET(TARGET_KEY_MANAGER_TEST "key-manager-tests")
PKG_CHECK_MODULES(KEY_MANAGER_TEST_DEP
	dlog
	openssl
	libsmack
	libsystemd-daemon
	REQUIRED
	)

SET(KEY_MANAGER_SRC_PATH ${PROJECT_SOURCE_DIR}/src)
SET(KEY_MANAGER_PATH ${PROJECT_SOURCE_DIR}/src/manager)

SET(KEY_MANAGER_TEST_SOURCES
	${KEY_MANAGER_PATH}/main/key-manager-util.cpp
	${KEY_MANAGER_PATH}/main/generic-socket-manager.cpp
	${KEY_MANAGER_PATH}/main/socket-manager.cpp
	${KEY_MANAGER_PATH}/service/echo.cpp
	${KEY_MANAGER_PATH}/service/ckm-service.cpp
	${KEY_MANAGER_PATH}/service/ckm-logic.cpp
	${KEY_MANAGER_PATH}/service/key-provider.cpp
	${KEY_MANAGER_PATH}/service/ocsp.cpp
	${KEY_MANAGER_PATH}/service/DBCryptoModule.cpp
	${KEY_MANAGER_PATH}/service/CryptoService.cpp
	${KEY_MANAGER_PATH}/service/file-system.cpp
	${KEY_MANAGER_PATH}/service/db-crypto.cpp

	${PROJECT_SOURCE_DIR}/tests/main.cpp
	)

SET_SOURCE_FILES_PROPERTIES(
	${KEY_MANAGER_TEST_SOURCES}
	PROPERTIES
		COMPILE_FLAGS "-D_GNU_SOURCE -fvisibility=hidden")

INCLUDE_DIRECTORIES(SYSTEM ${KEY_MANAGER_TEST_DEP_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(
	${KEY_MANAGER_SRC_PATH}/include
	${KEY_MANAGER_PATH}/main
	${KEY_MANAGER_PATH}/common
	${KEY_MANAGER_PATH}/service
	${KEY_MANAGER_PATH}/sqlcipher
	${KEY_MANAGER_PATH}/dpl/core/include
	${KEY_MANAGER_PATH}/dpl/log/include
	${KEY_MANAGER_PATH}/dpl/db/include
	)

ADD_EXECUTABLE(${TARGET_KEY_MANAGER_TEST} ${KEY_MANAGER_TEST_SOURCES})

TARGET_LINK_LIBRARIES(${TARGET_KEY_MANAGER_TEST}
	${KEY_MANAGER_TEST_DEP_LIBRARIES}
	${TARGET_KEY_MANAGER_COMMON}
	${TARGET_KEY_MANAGER_KEY_PROVIDER}
	)

INSTALL(TARGETS ${TARGET_KEY_MANAGER_TEST} DESTINATION bin)