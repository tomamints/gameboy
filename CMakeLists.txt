cmake_minimum_required(VERSION 3.10)
project(GameBoyEmulator)

set(CMAKE_CXX_STANDARD 17)

# ヘッダーファイルのディレクトリを追加
include_directories(include)

# srcフォルダのすべてのcppをコンパイル対象にする
file(GLOB SOURCES "src/*.cpp")

add_executable(gameboy ${SOURCES})
