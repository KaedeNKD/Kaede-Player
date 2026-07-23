cmake_minimum_required(VERSION 3.16)
project(KaedeDAW LANGUAGES CXX)

# =======================================================================
# ⚡ 全域編譯標準與防禦設定
# =======================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 👑 永久防禦：全域禁用 Windows.h 內建的 min/max 巨集
add_compile_definitions(NOMINMAX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ⚡ 新增 Sql 模組以驅動 SQLite 引擎
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Quick QuickWidgets Concurrent Sql)
find_package(TagLib REQUIRED)

set(BASS_DIR "${CMAKE_SOURCE_DIR}/bass")

# =======================================================================
# 👑 宿主核心 (Host)
# =======================================================================
set(HOST_SOURCES
    src/Host/main.cpp
    src/Host/MainWindow.h
    src/Host/MainWindow.cpp
    src/Host/GB.h
    src/Host/GB.cpp
    src/Host/DropZoneDialog.h
    src/Host/DropZoneDialog.cpp
    src/Host/ThemeManager.h
    src/Host/AdaptiveColorEngine.h
    
    # ⚡ 新增：獨立解耦的媒體庫視圖模組
    src/Host/Views/KaedeLibraryPanel.h
    src/Host/Views/KaedeLibraryPanel.cpp
    
    src/Host/PlaybackConsole.h       
    src/Host/PlaybackConsole.cpp     
    src/Host/PlayerExpandedPanel.h   
    src/Host/PlayerExpandedPanel.cpp 
    src/Host/ProgressBarIsland.h
    src/Host/ProgressBarIsland.cpp
    
    src/Host/DspVisualizerPanel.h
    src/Host/DspVisualizerPanel.cpp
    
    src/Host/KaedeDatabase.h
    src/Host/KaedeDatabase.cpp

    src/Host/TrackAnalyzerPanel.h
    src/Host/TrackAnalyzerPanel.cpp
    
    plugins/mod_player/VisualizerNodes.h
    plugins/mod_player/VisualizerNodes.cpp
    
    plugins/mod_player/KaedeAudioEngine.h
    plugins/mod_player/KaedeAudioEngine.cpp
    plugins/mod_player/MediaMetadataParser.h
    plugins/mod_player/MediaMetadataParser.cpp
    
    plugins/mod_player/TrackPeqPanel.h
    plugins/mod_player/TrackPeqPanel.cpp
    
    assets.qrc
)

if(WIN32)
    add_executable(KaedeHost WIN32 ${HOST_SOURCES})
else()
    add_executable(KaedeHost ${HOST_SOURCES})
endif()

target_include_directories(KaedeHost PRIVATE 
    ${CMAKE_SOURCE_DIR}/include 
    ${CMAKE_SOURCE_DIR}/plugins/mod_player 
    ${BASS_DIR}
)

target_link_libraries(KaedeHost PRIVATE
    Qt6::Core 
    Qt6::Gui 
    Qt6::Widgets
    Qt6::Quick
    Qt6::QuickWidgets
    Qt6::Concurrent
    Qt6::Sql
    TagLib::TagLib
    ${BASS_DIR}/bass.lib
    dwmapi.lib
    user32.lib
)

# =======================================================================
# 🚚 自動部署 (Copy DLLs to build directory)
# =======================================================================
if(WIN32)
    add_custom_command(TARGET KaedeHost POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${BASS_DIR}/bass.dll" $<TARGET_FILE_DIR:KaedeHost>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${BASS_DIR}/basswasapi.dll" $<TARGET_FILE_DIR:KaedeHost>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${BASS_DIR}/bassasio.dll" $<TARGET_FILE_DIR:KaedeHost>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${BASS_DIR}/bassdsd.dll" $<TARGET_FILE_DIR:KaedeHost> # ⚡ 部署 DSD 外掛
    )
endif()