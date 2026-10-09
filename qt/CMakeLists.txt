cmake_minimum_required(VERSION 3.16)
project(VibeStreamQt VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia Sql)

qt_add_executable(vibestream-qt
  src/main.cpp
  src/mainwindow.h src/mainwindow.cpp
  src/player.h src/player.cpp
  src/library.h src/library.cpp
  src/downloader.h src/downloader.cpp
)

target_link_libraries(vibestream-qt PRIVATE
  Qt6::Widgets Qt6::Multimedia Qt6::Sql)

# Build with:
#   vcvars64.bat && cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=D:/Qt/6.7.3/msvc2019_64
#   cmake --build build
#   windeployqt build\vibestream-qt.exe
