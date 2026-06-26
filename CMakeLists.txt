cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

find_package(obs-frontend-api REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets Qt6::Network)
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON)

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.cpp
    src/ChatBot.cpp
    src/YouTubeClient.cpp
    src/CommandHandler.cpp
    src/TimedMessages.cpp
    src/ui/ChatDock.cpp
    src/ui/SettingsDialog.cpp
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE src)

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE "PLUGIN_VERSION=\"${_version}\"")

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

# Bundle the Qt TLS Schannel backend — OBS does not ship it but Qt Network
# requires it for HTTPS. Install into tls/ next to the plugin DLL.
if(WIN32)
  get_filename_component(_qt6_root "${Qt6_DIR}/../../.." ABSOLUTE)
  set(_qt6_tls_dll "${_qt6_root}/plugins/tls/qschannelbackend.dll")
  if(EXISTS "${_qt6_tls_dll}")
    install(FILES "${_qt6_tls_dll}" DESTINATION "${_name}/bin/64bit/tls")
  else()
    message(WARNING "Qt TLS backend not found at ${_qt6_tls_dll} — HTTPS will not work")
  endif()
endif()
