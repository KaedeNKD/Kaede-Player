project(KaedePlayerPlugin LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Quick QuickWidgets)
find_package(TagLib REQUIRED)

# =======================================================================
# 🎸 播放器外掛原始碼 (乾淨版)
# =======================================================================
set(PLUGIN_SOURCES
    PlayerPlugin.h           
    KaedeAudioEngine.h
    KaedeAudioEngine.cpp
    MediaMetadataParser.h
    MediaMetadataParser.cpp

    VisualizerNodes.h
    VisualizerNodes.cpp
)

qt_add_plugin(mod_player SHARED ${PLUGIN_SOURCES})

set(BASS_DIR "${CMAKE_SOURCE_DIR}/bass")

target_include_directories(mod_player PRIVATE 
    ${CMAKE_SOURCE_DIR}/include 
    ${BASS_DIR}                 
)

target_link_libraries(mod_player PRIVATE
    Qt6::Core 
    Qt6::Gui 
    Qt6::Widgets 
    Qt6::Quick 
    Qt6::QuickWidgets
    TagLib::TagLib
    ${BASS_DIR}/bass.lib        
)