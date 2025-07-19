cmake_minimum_required(VERSION 3.16)
project(MeshApps)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ========================
# パッケージの検出
# ========================
find_package(glfw3 REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(OpenGL REQUIRED)

if(UNIX AND NOT APPLE)
  set(OpenGL_GL_PREFERENCE GLVND)
endif()

# ========================
# glad ライブラリ
# ========================
add_library(glad STATIC ${CMAKE_SOURCE_DIR}/common/common/external/glad/src/glad.c)
target_include_directories(glad PUBLIC ${CMAKE_SOURCE_DIR}/common/common/external/glad/include)

if(Eigen3_INCLUDE_DIR)
  include_directories(${Eigen3_INCLUDE_DIR})
elseif(EIGEN3_INCLUDE_DIR)
  include_directories(${EIGEN3_INCLUDE_DIR})
else()
  message(FATAL_ERROR "Eigen include directory not found!")
endif()

# ========================
# 共通インタフェースライブラリ（ヘッダのみ）
# ========================
add_library(mesh_common INTERFACE)

target_include_directories(mesh_common INTERFACE
  ${CMAKE_SOURCE_DIR}/common/common/render_Eigen
  ${CMAKE_SOURCE_DIR}/common/common/util
  ${CMAKE_SOURCE_DIR}/common/common/meshL
  ${CMAKE_SOURCE_DIR}/common/common/octree
  ${CMAKE_SOURCE_DIR}/common/common/kdtree2d
  ${EIGEN3_INCLUDE_DIR}
  ${OPENGL_INCLUDE_DIR}
)

# ========================
# 各課題ごとのビルド設定
# ========================

# 1. ccsub
add_executable(ccsub
  ccsub/main.cc
  ccsub/CCSubL.hxx
)
target_include_directories(ccsub PRIVATE ${CMAKE_SOURCE_DIR}/ccsub)
target_link_libraries(ccsub mesh_common glad glfw OpenGL::GL)

# 2. loopsub
add_executable(loopsub
  loopsub/main.cc
  loopsub/LoopSubL.hxx
)
target_include_directories(loopsub PRIVATE ${CMAKE_SOURCE_DIR}/loopsub)
target_link_libraries(loopsub mesh_common glad glfw OpenGL::GL)

# 3. kdtree2d
add_executable(kdtree2d
  kdtree2d/main.cc
  kdtree2d/KdTree.hxx
  ${CMAKE_SOURCE_DIR}/common/common/kdtree2d/GLKdTree.hxx
)
target_include_directories(kdtree2d PRIVATE ${CMAKE_SOURCE_DIR}/kdtree2d)
target_link_libraries(kdtree2d mesh_common glad glfw OpenGL::GL)

# 4. octree
add_executable(octree
  octree/main.cc
  ${CMAKE_SOURCE_DIR}/common/common/octree/raytri.c
  ${CMAKE_SOURCE_DIR}/common/common/octree/tribox3.c
)
target_include_directories(octree PRIVATE ${CMAKE_SOURCE_DIR}/octree)
target_link_libraries(octree mesh_common glad glfw OpenGL::GL)

# 5. smooth
add_executable(smooth
  smooth/main.cc
)
target_include_directories(smooth PRIVATE ${CMAKE_SOURCE_DIR}/smooth)
target_link_libraries(smooth mesh_common glad glfw OpenGL::GL)
