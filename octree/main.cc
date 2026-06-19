////////////////////////////////////////////////////////////////////
//
// $Id: main.cc 2026/06/19 21:29:43 kanai Exp 
//
// Copyright (c) 2022-2025 by Takashi Kanai. All rights reserved.
//
////////////////////////////////////////////////////////////////////

#include "envDep.h"
#include "mydef.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <chrono>
#include <memory>
#include <string>
#include <sstream>
#include <iomanip>
#include <random>
#include <vector>
#include <limits>
// using namespace std;

#include "myGL.hxx"
#include <GLFW/glfw3.h>

#include "MeshL.hxx"
#include "SMFLIO.hxx"

std::shared_ptr<MeshL> mesh;
SMFLIO smflio;

#include "Octree.hxx"
#include "GLOctree.hxx"

std::shared_ptr<Octree> octree;
GLOctree gloctree;

constexpr int NUM_RAYS = 1000;

struct Ray {
  Eigen::Vector3d pos;
  Eigen::Vector3d dir;
};

std::vector<Ray> rays;
std::vector<Eigen::Vector3d> ray_segments;
std::vector<Eigen::Vector3d> hit_points;

#include "GLMeshL.hxx"
#include "GLPanel.hxx"

GLPanel pane;
GLMeshL glmeshl;

////////////////////////////////////////////////////////////////////////////////////

#include "c11timer.hxx"

C11Timer c11fps;
double max_c11fps = 0.0;

////////////////////////////////////////////////////////////////////////////////////

// keyboard
bool shift_key_pressed = false;
// mouse
bool left_button_pressed = false;
bool right_button_pressed = false;

int width = 800;
int height = 800;

////////////////////////////////////////////////////////////////////////////////////

static void error_callback(int error, const char* description) {
  fputs(description, stderr);
}

static void framebuffer_size_callback(GLFWwindow* window, int width,
                                      int height) {
  glViewport(0, 0, width, height);
}

// キーボードイベント処理関数
static void key_callback(GLFWwindow* window, int key, int scancode, int action,
                         int mods) {
  // ESC
  if ((key == GLFW_KEY_ESCAPE) && (action == GLFW_PRESS)) {
    glfwSetWindowShouldClose(window, GL_TRUE);
    return;
  }

  // q
  else if ((key == GLFW_KEY_Q) && (action == GLFW_PRESS)) {
    glfwSetWindowShouldClose(window, GL_TRUE);
    return;
  }

  // 1 (smooth shading)
  else if ((key == GLFW_KEY_1) && (action == GLFW_PRESS)) {
    glmeshl.setIsSmoothShading(true);
    glmeshl.setIsDrawWireframe(false);
    return;
  }

  // 2 (flat shading)
  else if ((key == GLFW_KEY_2) && (action == GLFW_PRESS)) {
    glmeshl.setIsSmoothShading(false);
    glmeshl.setIsDrawWireframe(false);
    return;
  }

  // 3 (flat + wireframe)
  else if ((key == GLFW_KEY_3) && (action == GLFW_PRESS)) {
    glmeshl.setIsSmoothShading(false);
    glmeshl.setIsDrawWireframe(true);
    return;
  }

  // 4 (gradient background)
  else if ((key == GLFW_KEY_4) && (action == GLFW_PRESS)) {
    if (pane.isGradientBackground() == false) {
      pane.setIsGradientBackground(true);
    } else {
      pane.setIsGradientBackground(false);
    }
    return;
  }

  // shift
  else if ((key == GLFW_KEY_LEFT_SHIFT) && (action == GLFW_PRESS)) {
    shift_key_pressed = true;
    return;
  } else if ((key == GLFW_KEY_LEFT_SHIFT) && (action == GLFW_RELEASE)) {
    shift_key_pressed = false;
    return;
  }

}

// マウスイベント処理関数
static void mousebutton_callback(GLFWwindow* window, int button, int action,
                                 int mods) {
  double xd, yd;
  glfwGetCursorPos(window, &xd, &yd);
  pane.setScreenXY((int)xd, (int)yd);

  if ((button == GLFW_MOUSE_BUTTON_1) && (action == GLFW_PRESS)) {
    left_button_pressed = true;
    pane.startRotate();
    pane.startZoom();
    pane.startMove();
  } else if ((button == GLFW_MOUSE_BUTTON_1) && (action == GLFW_RELEASE)) {
    left_button_pressed = false;
    pane.finishRMZ();
  } else if ((button == GLFW_MOUSE_BUTTON_2) && (action == GLFW_PRESS)) {
    right_button_pressed = true;
  } else if ((button == GLFW_MOUSE_BUTTON_2) && (action == GLFW_RELEASE)) {
    right_button_pressed = false;
  }
}

// カーソルイベント処理関数
static void cursorpos_callback(GLFWwindow* window, double xd, double yd) {
  int x = (int)xd;
  int y = (int)yd;

  const bool ctrl_held =
      glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;

  if (left_button_pressed && !shift_key_pressed && !ctrl_held) {
    pane.updateRotate(x, y);
  } else if (left_button_pressed && shift_key_pressed && !ctrl_held) {
    pane.updateZoom(x, y);
  } else if (left_button_pressed && !shift_key_pressed && ctrl_held) {
    pane.updateMove(x, y);
  }
}

// mouse wheel
static void scroll_callback(GLFWwindow* window, double xoffset,
                            double yoffset) {
  pane.updateWheelZoom(yoffset);
}

// window resize
static void windowsize_callback(GLFWwindow* window, int w, int h) {
  width = w;
  height = h;
  pane.changeSize(w, h);
}

////////////////////////////////////////////////////////////////////////////////////

bool rayAABBInterval(const Eigen::Vector3d& pos, const Eigen::Vector3d& dir,
                     const Eigen::Vector3d& bbmin, const Eigen::Vector3d& bbmax,
                     double& t_enter, double& t_exit) {
  double t_max = std::numeric_limits<double>::max();
  double t_min = -std::numeric_limits<double>::max();

  for (int i = 0; i < 3; ++i) {
    double t1, t2;
    if (i == 0) {
      if (std::abs(dir.x()) < 1e-10) {
        if (pos.x() < bbmin.x() || pos.x() > bbmax.x()) return false;
        continue;
      }
      t1 = (bbmin.x() - pos.x()) / dir.x();
      t2 = (bbmax.x() - pos.x()) / dir.x();
    } else if (i == 1) {
      if (std::abs(dir.y()) < 1e-10) {
        if (pos.y() < bbmin.y() || pos.y() > bbmax.y()) return false;
        continue;
      }
      t1 = (bbmin.y() - pos.y()) / dir.y();
      t2 = (bbmax.y() - pos.y()) / dir.y();
    } else {
      if (std::abs(dir.z()) < 1e-10) {
        if (pos.z() < bbmin.z() || pos.z() > bbmax.z()) return false;
        continue;
      }
      t1 = (bbmin.z() - pos.z()) / dir.z();
      t2 = (bbmax.z() - pos.z()) / dir.z();
    }

    const double t_near = std::min(t1, t2);
    const double t_far = std::max(t1, t2);
    t_max = std::min(t_max, t_far);
    t_min = std::max(t_min, t_near);
    if (t_min > t_max) return false;
  }

  t_enter = t_min;
  t_exit = t_max;
  return true;
}

Eigen::Vector3d randomPointInBox(const Eigen::Vector3d& bbmin,
                                   const Eigen::Vector3d& bbmax,
                                   std::mt19937& rng) {
  std::uniform_real_distribution<double> dist(0.0, 1.0);
  return bbmin + dist(rng) * (bbmax - bbmin);
}

Eigen::Vector3d randomUnitVector(std::mt19937& rng) {
  std::uniform_real_distribution<double> dist(-1.0, 1.0);
  Eigen::Vector3d v;
  do {
    v << dist(rng), dist(rng), dist(rng);
  } while (v.squaredNorm() < 1e-12);
  return v.normalized();
}

void generateRandomRays(const Eigen::Vector3d& bbmin,
                        const Eigen::Vector3d& bbmax, int num_rays) {
  std::random_device rd;
  std::mt19937 rng(rd());

  const Eigen::Vector3d extent = bbmax - bbmin;
  const double outside_dist = extent.norm();

  rays.clear();
  rays.reserve(num_rays);
  ray_segments.clear();
  ray_segments.reserve(num_rays * 2);

  for (int i = 0; i < num_rays; ++i) {
    const Eigen::Vector3d target = randomPointInBox(bbmin, bbmax, rng);
    const Eigen::Vector3d pos = target + randomUnitVector(rng) * outside_dist;
    const Eigen::Vector3d dir = (target - pos).normalized();

    double t_enter, t_exit;
    if (!rayAABBInterval(pos, dir, bbmin, bbmax, t_enter, t_exit)) continue;

    rays.push_back({pos, dir});
    ray_segments.push_back(pos + t_enter * dir);
    ray_segments.push_back(pos + t_exit * dir);
  }

  std::cout << rays.size() << " rays generated through the bounding box."
            << std::endl;
}

////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " in.obj" << std::endl;
    return EXIT_FAILURE;
  }

  // mesh の読み込み
  mesh = std::make_shared<MeshL>();
  smflio.setMesh(*mesh);
  if (smflio.inputFromFile(argv[1]) == false) {
    return EXIT_FAILURE;
  }
  mesh->calcSmoothVertexNormal();

  //
  // ここから

  // octree の初期化
  octree = std::make_shared<Octree>();

  // mesh の Bounding Box を計算
  Eigen::Vector3d bbmin, bbmax;
  mesh->computeBB(bbmin, bbmax);
  octree->setBB(bbmin, bbmax);

  // octree の構築
  // 面を格納 （addFaceToOctree を利用）

  // レイの生成 (generateRandomRays を利用)
  // rays （グローバル変数）にレイが格納される
  generateRandomRays(bbmin, bbmax, NUM_RAYS);

  // 1. Octree を利用して各レイとメッシュの交点を求める
  //    関数を別途作成して呼び出す

  // 2. 面を全探索
  //    関数を別途作成して呼び出す

  // ここまで

  //
  // 表示用設定 （ここから先は特に触らなくても良い）
  //

  // ここからウインドウの初期化処理
  glfwSetErrorCallback(error_callback);
  if (!glfwInit()) return EXIT_FAILURE;

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_SAMPLES, 4);  // MSAA 4xサンプリング

  GLFWwindow* window =
      glfwCreateWindow(width, height, "GLFW Window", nullptr, nullptr);
  if (!window) {
    std::cerr << "Failed to create GLFW window" << std::endl;
    const char* error;
    glfwGetError(&error);
    if (error) {
      std::cerr << "GLFW Error: " << error << std::endl;
    }
    glfwTerminate();
    return EXIT_FAILURE;
  }

  glfwMakeContextCurrent(window);

  // 垂直同期（VSync）を有効にし，画面更新をディスプレイのリフレッシュレートに同期
  glfwSwapInterval(1);

  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  glfwSetKeyCallback(window, key_callback);
  glfwSetMouseButtonCallback(window, mousebutton_callback);
  glfwSetCursorPosCallback(window, cursorpos_callback);
  glfwSetScrollCallback(window, scroll_callback);
  glfwSetWindowSizeCallback(window, windowsize_callback);

  glfwShowWindow(window);
  glfwFocusWindow(window);

  // OpenGLの初期化
  pane.init(width, height);
  pane.initGL();
  pane.initShader();

  // 初期ビューポート設定（Mac
  // でウインドウが小さく表示されるのを避けるために必須）
  int fbWidth, fbHeight;
  glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
  glViewport(0, 0, fbWidth, fbHeight);

  // ここまでウインドウの初期化処理

  // メッシュ表示用 に mesh をセット
  glmeshl.setMesh(mesh);

  // Octree 表示用に octree をセット
  gloctree.setOctree(octree);
  gloctree.init3d(pane.shader());
  gloctree.initRayIntersections(ray_segments, hit_points);

  c11fps.ResetFPS();

  // 描画ループ処理
  while (!glfwWindowShouldClose(window)) {

    // 画面のクリア・初期化
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    pane.clear(fbWidth, fbHeight);
    pane.update(glmeshl.material());

    glmeshl.draw(pane.shader());

    gloctree.drawOctree();

    gloctree.drawRayIntersection();

    pane.finish();

    // fps 計測
    double f = c11fps.CheckGetFPS();
    if ( max_c11fps < f ) max_c11fps = f;
    
    std::stringstream ss;
    ss << std::fixed << std::setprecision(3) << std::setw(8) << f << " fps - max " << std::setw(8) << max_c11fps << " fps";
    std::string buf = ss.str();
    
    std::string txt = "GLFW Window - " + buf;
    glfwSetWindowTitle( window, txt.c_str() );
    
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwDestroyWindow(window);
  glfwTerminate();

  return EXIT_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////
