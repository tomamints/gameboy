cmake_minimum_required(VERSION 3.10)
project(GameBoyEmulator)

set(CMAKE_CXX_STANDARD 17)

# SDL2を探す
find_package(SDL2 REQUIRED)

# ヘッダーファイルのディレクトリを追加
include_directories(include)
include_directories(${SDL2_INCLUDE_DIRS})

# srcフォルダのすべてのcppをコンパイル対象にする
file(GLOB SOURCES "src/*.cpp")

add_executable(gameboy ${SOURCES})

# SDL2をリンク
target_link_libraries(gameboy ${SDL2_LIBRARIES})
