include(ExternalProject)

### libjpeg-turbo ###################################################
ExternalProject_Add (libjpeg-turbo
    PREFIX libjpeg-turbo
    GIT_REPOSITORY "https://github.com/libjpeg-turbo/libjpeg-turbo.git"
    GIT_TAG "main"
    SOURCE_DIR "${CMAKE_BINARY_DIR}/third-party/libjpeg-turbo"
    CMAKE_ARGS "-DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/libjpeg-turbo"
      "-DCMAKE_GENERATOR=${CMAKE_GENERATOR}"
      "-DCMAKE_POSITION_INDEPENDENT_CODE=ON"
      "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
)