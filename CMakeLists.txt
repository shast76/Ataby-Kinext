cmake_minimum_required(VERSION 3.20)
project(Kinext VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ONNX Runtime klasoru (NuGet paketinden cikarilan ya da GitHub release zip'i)
set(ONNXRUNTIME_DIR "${CMAKE_SOURCE_DIR}/third_party/onnxruntime" CACHE PATH "ONNX Runtime kok klasoru")
if(WIN32)
    option(USE_DIRECTML "GPU icin DirectML kullan (Microsoft.ML.OnnxRuntime.DirectML paketi)" ON)
else()
    set(USE_DIRECTML OFF)
endif()

find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui videoio)

find_path(ORT_INCLUDE onnxruntime_cxx_api.h
    HINTS ${ONNXRUNTIME_DIR}/include
          ${ONNXRUNTIME_DIR}/build/native/include
          ${ONNXRUNTIME_DIR}/include/onnxruntime/core/session
    REQUIRED)
find_library(ORT_LIB onnxruntime
    HINTS ${ONNXRUNTIME_DIR}/lib
          ${ONNXRUNTIME_DIR}/runtimes/win-x64/native
          ${ONNXRUNTIME_DIR}/runtimes/linux-x64/native
    REQUIRED)

# --- Kinext Core: poz tahmini + iskelet + jestler (oyunlarin ortak parcasi) ---
add_library(kinext_core STATIC src/pose.cpp)
target_include_directories(kinext_core PUBLIC src ${ORT_INCLUDE} ${OpenCV_INCLUDE_DIRS})
target_link_libraries(kinext_core PUBLIC ${ORT_LIB} ${OpenCV_LIBS})
if(USE_DIRECTML)
    target_compile_definitions(kinext_core PUBLIC USE_DIRECTML)
endif()
if(WIN32)
    # Kamera isimleri icin Media Foundation
    target_link_libraries(kinext_core PUBLIC mfplat mf mfuuid ole32)
    target_compile_definitions(kinext_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# --- Programlar ---
add_executable(kinext src/main.cpp)              # iskelet test araci
target_link_libraries(kinext PRIVATE kinext_core)

add_executable(kinext_games src/games_main.cpp)  # oyunlar
target_link_libraries(kinext_games PRIVATE kinext_core)

# Windows: onnxruntime.dll (+ DirectML.dll) ve modelleri exe'lerin yanina kopyala
foreach(app kinext kinext_games)
    if(WIN32)
        get_filename_component(ORT_LIB_DIR ${ORT_LIB} DIRECTORY)
        file(GLOB ORT_DLLS "${ORT_LIB_DIR}/*.dll")
        foreach(dll ${ORT_DLLS})
            add_custom_command(TARGET ${app} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${dll} $<TARGET_FILE_DIR:${app}>)
        endforeach()
    endif()
    add_custom_command(TARGET ${app} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/models $<TARGET_FILE_DIR:${app}>/models)
endforeach()
