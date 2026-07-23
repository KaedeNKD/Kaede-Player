# ==========================================
# KAEDE DAW - PLUGINS ROOT CMAKE
# ==========================================
# 確保 Qt 的自動編譯工具開啟
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 註冊第一個萬能播放器組件
add_subdirectory(mod_player)