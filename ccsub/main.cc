////////////////////////////////////////////////////////////////////
//
// $Id: main.cc 2025/07/19 15:01:43 kanai Exp 
//
// Copyright (c) 2022-2025 by Takashi Kanai. All rights reserved.
//
////////////////////////////////////////////////////////////////////

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>

#include "envDep.h"
#include "mydef.h"
//using namespace std;

#include "myGL.hxx"
#include <GLFW/glfw3.h>

#include "CCSubL.hxx"
#include "MeshL.hxx"
#include "SMFLIO.hxx"

std::vector<std::shared_ptr<MeshL> > mesh;
int mno = 0;
SMFLIO smflio;

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
bool control_key_pressed = false;
// mouse
bool left_button_pressed = false;
bool right_button_pressed = false;

int width = 800;
int height = 800;

////////////////////////////////////////////////////////////////////////////////////

static void error_callback(int error, const char* description) {
  fputs(description, stderr);
}

static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
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

  // m
  else if ((key == GLFW_KEY_M) && (action == GLFW_PRESS)) {
    mno++;
    if (mno == mesh.size()) {
      std::shared_ptr<MeshL> mesh1 = std::make_shared<MeshL>();
      mesh.push_back( mesh1 );
      CCSubL ccsub( *(mesh[mno-1]), *(mesh[mno]) );
      ccsub.apply();
    }
    glmeshl.setMesh( mesh[mno] );
    //glmeshl.buildBuffers();
    return;
  }

  // n
  else if ((key == GLFW_KEY_N) && (action == GLFW_PRESS)) {
    if (mno == 0) return;
    mno--;
    glmeshl.setMesh( mesh[mno] );
    //glmeshl.buildBuffers();
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

  // control
  else if ((key == GLFW_KEY_LEFT_CONTROL) && (action == GLFW_PRESS)) {
    control_key_pressed = true;
    return;
  } else if ((key == GLFW_KEY_LEFT_CONTROL) && (action == GLFW_RELEASE)) {
    control_key_pressed = false;
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

  if (left_button_pressed && !shift_key_pressed && !control_key_pressed) {
    pane.updateRotate(x, y);
  } else if (left_button_pressed && shift_key_pressed && !control_key_pressed) {
    pane.updateZoom(x, y);
  } else if (left_button_pressed && !shift_key_pressed && control_key_pressed) {
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

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " in.obj" << std::endl;
    return EXIT_FAILURE;
  }

  // メッシュデータの読み込み
  std::shared_ptr<MeshL> mesh0 = std::make_shared<MeshL>();
  smflio.setMesh(*mesh0);
  if (smflio.inputFromFile(argv[1]) == false) {
     return EXIT_FAILURE;
  }
  
  // 接続性の構築（ハーフエッジのmate関係を設定）
  mesh0->createConnectivity(true);
  
  mesh0->calcSmoothVertexNormal();
  mesh.push_back(mesh0);

  // ここからウインドウの初期化処理
  glfwSetErrorCallback(error_callback);
  if (!glfwInit()) return EXIT_FAILURE;

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_SAMPLES, 4);  // MSAA 4xサンプリング

  GLFWwindow* window =
    glfwCreateWindow(width, height, "GLFW Window", NULL, NULL);
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

  // 初期ビューポート設定（Mac でウインドウが小さく表示されるのを避けるために必須）
  int fbWidth, fbHeight;
  glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
  glViewport(0, 0, fbWidth, fbHeight);

  glfwSwapInterval(0);
  // ここまでウインドウの初期化処理

  // メッシュ表示用 に mesh をセット
  glmeshl.setMesh(mesh[0]);
//  glmeshl.buildBuffers();

  c11fps.ResetFPS();
  
  // 描画ループ処理
  while (!glfwWindowShouldClose(window)) {

    // 画面のクリア・初期化
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    pane.clear(fbWidth, fbHeight);
    pane.update(glmeshl.material());

    glmeshl.draw(pane.shader());

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

  // ウインドウ終了処理
  glfwDestroyWindow(window);
  glfwTerminate();

  return EXIT_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////
