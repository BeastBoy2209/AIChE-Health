set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)
add_library(imgui STATIC
  ${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui_draw.cpp ${IMGUI_DIR}/imgui_tables.cpp ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

add_executable(aiche-health
  main.cpp app.cpp api.cpp theme.cpp widgets.cpp screen_overview.cpp screen_vitals.cpp screen_mind.cpp screen_body.cpp screen_advisor.cpp screen_moca.cpp screen_setup.cpp)
target_include_directories(aiche-health PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(aiche-health PRIVATE imgui aiche_core)
if(WIN32)
  set_target_properties(aiche-health PROPERTIES WIN32_EXECUTABLE OFF)
endif()

