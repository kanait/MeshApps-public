////////////////////////////////////////////////////////////////////
//
// Tutte parameterization viewer
//
// Copyright (c) 2025 Takashi Kanai
// Released under the MIT license
//
////////////////////////////////////////////////////////////////////

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <iomanip>
#include <vector>

#include "envDep.h"
#include "mydef.h"

#include "myGL.hxx"
#include <GLFW/glfw3.h>

#include "MeshL.hxx"
#include "SMFLIO.hxx"
#include "TutteParam.hxx"
#include "GLShader.hxx"
#include "GLMeshL.hxx"
#include "GLParamMeshL.hxx"
#include "GLPanel.hxx"

std::shared_ptr<MeshL> mesh;

GLPanel pane;
GLMeshL glmeshl;
GLParamMeshL glparam;

enum class DisplayMode { Mesh3D, Param2D };
DisplayMode display_mode = DisplayMode::Mesh3D;
bool parameterization_ready = false;

#include "c11timer.hxx"

C11Timer c11fps;
double max_c11fps = 0.0;

bool shift_key_pressed = false;
bool left_button_pressed = false;

int width = 1024;
int height = 1024;

struct PanelViewState {
  Eigen::Vector3f view_point;
  Eigen::Vector3f look_point;
  Eigen::Vector3f view_vector;
  Eigen::Quaternionf arcball_q;
  Eigen::Matrix4f arcball_m;
  Eigen::Vector3f arcball_offset;
  float arcball_seezo = 0.0f;
};

static PanelViewState view_state_3d;
static PanelViewState view_state_2d;
static bool view_state_3d_valid = false;
static bool view_state_2d_valid = false;

static void savePanelViewState(GLPanel& panel, PanelViewState& state) {
  state.view_point = panel.view_point();
  state.look_point = panel.look_point();
  state.view_vector = panel.view_vector();
  state.arcball_q = panel.manip().qNow();
  state.arcball_m = panel.manip().mNow();
  state.arcball_offset = panel.manip().offset();
  state.arcball_seezo = panel.manip().seezo();
}

static void restorePanelViewState(GLPanel& panel, const PanelViewState& state) {
  panel.setViewPoint(state.view_point.x(), state.view_point.y(),
                     state.view_point.z());
  panel.setLookPoint(state.look_point.x(), state.look_point.y(),
                     state.look_point.z());
  panel.setViewVector(state.view_vector.x(), state.view_vector.y(),
                      state.view_vector.z());
  panel.manip().restoreState(state.arcball_q, state.arcball_m,
                             state.arcball_offset, state.arcball_seezo);
}

static void resetPanelManipulator(GLPanel& panel) {
  panel.manip().init();
  const int hw = std::max(panel.w() / 2, 1);
  const int hh = std::max(panel.h() / 2, 1);
  panel.manip().setHalfWHL(hw, hh);
}

// UV の範囲に合わせて 2D パラメータ表示用のカメラを置く（3D の Arcball 状態は引き継がない）
static void fitViewToUV(const std::shared_ptr<MeshL>& m, GLPanel& panel) {
  if (!m || m->texcoords().empty()) return;

  resetPanelManipulator(panel);

  Eigen::Vector2d vmin, vmax;
  bool first = true;
  for (auto& tc : m->texcoords()) {
    const Eigen::Vector3d& p = tc->point();
    const Eigen::Vector2d uv(p.x(), p.y());
    if (first) {
      vmin = vmax = uv;
      first = false;
    } else {
      vmin = vmin.cwiseMin(uv);
      vmax = vmax.cwiseMax(uv);
    }
  }

  const Eigen::Vector2d center = 0.5 * (vmin + vmax);
  const double radius = 0.5 * (vmax - vmin).norm();
  if (radius < 1.0e-8) return;

  const float fov_rad =
      panel.fov() * static_cast<float>(M_PI) / 180.0f;
  const float aspect = std::max(panel.aspect(), 1.0e-3f);
  const float half_h = static_cast<float>(radius);

  const float dist_v = half_h / std::tan(0.5f * fov_rad);
  const float hfov_rad =
      2.0f * std::atan(std::tan(0.5f * fov_rad) * aspect);
  const float dist_h = half_h / std::tan(0.5f * hfov_rad);

  constexpr float kMargin = 1.35f;
  const float distance = kMargin * std::max(dist_v, dist_h);

  const float cx = static_cast<float>(center.x());
  const float cy = static_cast<float>(center.y());
  panel.setLookPoint(cx, cy, 0.0f);
  panel.setViewPoint(cx, cy, distance);
  panel.setViewVector(0.0f, 0.0f, distance);
  panel.initLightPositions();
}

// UV 平面表示から 3D に戻るときだけ、保存済みの 3D 視点を復元する。
static void switchTo3DView() {
  if (display_mode != DisplayMode::Param2D) return;

  savePanelViewState(pane, view_state_2d);
  view_state_2d_valid = true;
  display_mode = DisplayMode::Mesh3D;
  if (view_state_3d_valid) {
    restorePanelViewState(pane, view_state_3d);
  }
}

static void switchToParamView() {
  if (display_mode != DisplayMode::Mesh3D) return;
  if (!parameterization_ready) return;

  savePanelViewState(pane, view_state_3d);
  view_state_3d_valid = true;
  display_mode = DisplayMode::Param2D;
  glparam.setIsDrawWireframe(true);
  if (view_state_2d_valid) {
    restorePanelViewState(pane, view_state_2d);
  } else {
    fitViewToUV(mesh, pane);
    savePanelViewState(pane, view_state_2d);
    view_state_2d_valid = true;
  }
}

// Procedural checkerboard for UV distortion visualization (REPEAT in UV space).
static unsigned int createCheckerTexture(int size = 128, int cell_size = 4) {
  if (cell_size < 1) cell_size = 1;
  std::vector<unsigned char> pixels(static_cast<size_t>(size) * size * 3);
  const unsigned char blue[3] = {72, 118, 210};
  const unsigned char orange[3] = {245, 148, 58};
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      const bool use_blue = ((x / cell_size) + (y / cell_size)) % 2 == 0;
      const unsigned char* rgb = use_blue ? blue : orange;
      const size_t idx = static_cast<size_t>(3 * (y * size + x));
      pixels[idx] = rgb[0];
      pixels[idx + 1] = rgb[1];
      pixels[idx + 2] = rgb[2];
    }
  }

  GLuint tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, size, size, 0, GL_RGB,
               GL_UNSIGNED_BYTE, pixels.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glBindTexture(GL_TEXTURE_2D, 0);
  return static_cast<unsigned int>(tex);
}

static void error_callback(int error, const char* description) {
  fputs(description, stderr);
}

static void framebuffer_size_callback(GLFWwindow* window, int w, int h) {
  glViewport(0, 0, w, h);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action,
                         int mods) {
  if ((key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q) && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, GL_TRUE);
    return;
  }
  if ((key == GLFW_KEY_1) && (action == GLFW_PRESS)) {
    switchTo3DView();
    if (!parameterization_ready || !glmeshl.hasTexturedBuffer()) {
      glmeshl.setIsDrawTexture(false);
      glmeshl.setIsSmoothShading(true);
      return;
    }
    glmeshl.setIsDrawTexture(!glmeshl.isDrawTexture());
    if (glmeshl.isDrawTexture()) glmeshl.setIsSmoothShading(false);
    return;
  }
  if ((key == GLFW_KEY_2) && (action == GLFW_PRESS)) {
    switchTo3DView();
    if (glmeshl.isDrawTexture()) {
      glmeshl.setIsDrawTexture(false);
      glmeshl.setIsSmoothShading(true);
    } else {
      glmeshl.setIsSmoothShading(!glmeshl.isSmoothShading());
    }
    return;
  }
  if ((key == GLFW_KEY_3) && (action == GLFW_PRESS)) {
    if (display_mode == DisplayMode::Param2D) {
      glparam.setIsDrawWireframe(!glparam.isDrawWireframe());
      return;
    }
    glmeshl.setIsDrawWireframe(!glmeshl.isDrawWireframe());
    return;
  }
  if ((key == GLFW_KEY_4) && (action == GLFW_PRESS)) {
    pane.setIsGradientBackground(!pane.isGradientBackground());
    return;
  }
  if ((key == GLFW_KEY_5) && (action == GLFW_PRESS)) {
    if (display_mode == DisplayMode::Mesh3D)
      switchToParamView();
    else
      switchTo3DView();
    return;
  }
  if ((key == GLFW_KEY_LEFT_SHIFT) && (action == GLFW_PRESS)) {
    shift_key_pressed = true;
    return;
  }
  if ((key == GLFW_KEY_LEFT_SHIFT) && (action == GLFW_RELEASE)) {
    shift_key_pressed = false;
    return;
  }
}

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
  }
}

static void cursorpos_callback(GLFWwindow* window, double xd, double yd) {
  const int x = (int)xd;
  const int y = (int)yd;
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

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
  pane.updateWheelZoom((float)yoffset);
}

static void windowsize_callback(GLFWwindow* window, int w, int h) {
  width = w;
  height = h;
  pane.changeSize(w, h);
  if (display_mode == DisplayMode::Param2D) {
    fitViewToUV(mesh, pane);
    savePanelViewState(pane, view_state_2d);
    view_state_2d_valid = true;
  }
}

int main(int argc, char** argv) {
  if (argc < 2 || argc > 3) {
    std::cerr << "Usage: " << argv[0] << " in.obj [out.obj]" << std::endl;
    return EXIT_FAILURE;
  }

  mesh = std::make_shared<MeshL>();
  SMFLIO smflio;
  smflio.setMesh(*mesh);
  if (!smflio.inputFromFile(argv[1])) {
    return EXIT_FAILURE;
  }

  //mesh->normalize();

  TutteParam param(mesh);
  parameterization_ready = param.apply();
  if (!parameterization_ready) {
    std::cerr << "tutteparam: parameterization is incomplete; opening shaded "
                 "mesh viewer."
              << std::endl;
  }

  if (argc == 3) {
    if (!parameterization_ready) {
      std::cerr << "tutteparam: skip writing " << argv[2]
                << " because UV parameterization is not available."
                << std::endl;
    } else {
      smflio.setSaveTexcoord(true);
      smflio.setSaveBLoop(true);
      if (!smflio.outputToFile(argv[2], true, true, true)) {
        std::cerr << "Failed to write " << argv[2] << std::endl;
        return EXIT_FAILURE;
      }
      std::cout << "Wrote " << argv[2] << std::endl;
    }
  }

  glfwSetErrorCallback(error_callback);
  if (!glfwInit()) return EXIT_FAILURE;

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_SAMPLES, 4);

  GLFWwindow* window =
      glfwCreateWindow(width, height, "tutteparam", NULL, NULL);
  if (!window) {
    std::cerr << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
    return EXIT_FAILURE;
  }

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  glfwSetKeyCallback(window, key_callback);
  glfwSetMouseButtonCallback(window, mousebutton_callback);
  glfwSetCursorPosCallback(window, cursorpos_callback);
  glfwSetScrollCallback(window, scroll_callback);
  glfwSetWindowSizeCallback(window, windowsize_callback);

  glfwShowWindow(window);
  glfwFocusWindow(window);

  pane.init(width, height);
  pane.initGL();
  pane.initShader();

  const unsigned int tex_id = createCheckerTexture();

  int fbWidth, fbHeight;
  glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
  glViewport(0, 0, fbWidth, fbHeight);
  pane.changeSize(fbWidth, fbHeight);

  {
    float mtl[17] = {0.2f, 0.2f, 0.2f, 1.0f, 0.8f, 0.8f, 0.8f, 1.0f,
                     0.0f, 0.0f, 0.0f, 1.0f, 0.35f, 0.35f, 0.35f, 1.0f,
                     64.0f};
    glmeshl.material().set(mtl);
  }

  glmeshl.setMesh(mesh);
  glparam.setMesh(mesh);
  {
    float param_mtl[17] = {0.15f, 0.15f, 0.2f, 1.0f, 0.75f, 0.85f, 0.95f, 1.0f,
                           0.0f, 0.0f, 0.0f, 1.0f, 0.2f, 0.2f, 0.2f, 1.0f,
                           32.0f};
    glparam.material().set(param_mtl);
  }
  glmeshl.setTexID(tex_id);
  parameterization_ready =
      parameterization_ready && glmeshl.hasTexturedBuffer() &&
      !mesh->texcoords().empty();
  if (parameterization_ready) {
    glmeshl.setIsDrawTexture(true);
    glmeshl.setIsSmoothShading(false);
  } else {
    display_mode = DisplayMode::Mesh3D;
    glmeshl.setIsDrawTexture(false);
    glmeshl.setIsSmoothShading(true);
  }

  c11fps.ResetFPS();

  while (!glfwWindowShouldClose(window)) {
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    pane.clear(fbWidth, fbHeight);

    if (display_mode == DisplayMode::Param2D) {
      pane.update(glparam.material());
      glparam.draw(pane.shader());
      if (glparam.isDrawWireframe()) {
        glparam.drawWireOverlay(pane.shader());
      }
    } else if (glmeshl.isDrawTexture() && glmeshl.hasTexturedBuffer()) {
      pane.updateTexture(glmeshl.material());
      glmeshl.drawTextured(pane.shader());
      if (glmeshl.isDrawWireframe()) {
        glmeshl.drawWireOverlay(pane.shader());
      }
    } else {
      pane.update(glmeshl.material());
      glmeshl.draw(pane.shader());
    }

    pane.finish();

    double f = c11fps.CheckGetFPS();
    if (max_c11fps < f) max_c11fps = f;

    std::stringstream ss;
    ss << std::fixed << std::setprecision(3) << std::setw(8) << f
       << " fps - max " << std::setw(8) << max_c11fps << " fps";
    const char* mode_txt =
        (display_mode == DisplayMode::Param2D) ? "UV param" : "3D mesh";
    glfwSetWindowTitle(window,
                       (std::string("tutteparam [") + mode_txt + "] - " + ss.str())
                           .c_str());

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwMakeContextCurrent(window);
  glmeshl.deleteVAOVBO();
  glparam.deleteVAOVBO();
  if (tex_id != 0) glDeleteTextures(1, &tex_id);

  glfwDestroyWindow(window);
  glfwTerminate();

  return EXIT_SUCCESS;
}
