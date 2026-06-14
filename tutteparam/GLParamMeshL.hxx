////////////////////////////////////////////////////////////////////
//
// UV parameter domain viewer for MeshL (2D layout of computed UVs)
//
// Copyright (c) 2025 Takashi Kanai
// Released under the MIT license
//
////////////////////////////////////////////////////////////////////

#ifndef _GLPARAMMESHL_HXX
#define _GLPARAMMESHL_HXX 1

#include <cstddef>
#include <iostream>
#include <memory>
#include <set>
#include <vector>

#include "myGL.hxx"

#include "GLMaterial.hxx"
#include "GLMesh.hxx"
#include "GLShader.hxx"
#include "MeshL.hxx"

struct ParamVertexAttrib {
  Eigen::Vector3f position;
  Eigen::Vector3f normal;
};

class GLParamMeshL : public GLMesh {
 public:
  GLParamMeshL() : mesh_(nullptr) {};
  ~GLParamMeshL() { resetVAOVBOHandles(); };

  void deleteVAOVBO() {
    if (vao_flat_ != 0) glDeleteVertexArrays(1, &vao_flat_);
    if (vbo_flat_ != 0) glDeleteBuffers(1, &vbo_flat_);
    if (vao_wire_ != 0) glDeleteVertexArrays(1, &vao_wire_);
    if (vbo_wire_ != 0) glDeleteBuffers(1, &vbo_wire_);
    resetVAOVBOHandles();
  };

  void setMesh(std::shared_ptr<MeshL> mesh) {
    mesh_ = mesh;
    buildBuffers();
  };

  void draw(GLShader& shader) {
    if (vao_flat_ == 0 || vertex_count_flat_ == 0) return;
    glUseProgram(shader.phongShaderProgram);
    glBindVertexArray(vao_flat_);
    glDrawArrays(GL_TRIANGLES, 0, vertex_count_flat_);
    glBindVertexArray(0);
  };

  void drawWireOverlay(GLShader& shader) {
    if (vao_wire_ == 0 || vertex_count_wire_ == 0) return;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glLineWidth(1.0f);

    glUseProgram(shader.lines3dShaderProgram);
    glUniform1f(shader.lines3dLineWidthLoc, 1.0f);
    glUniform3f(shader.lines3dLineColorLoc, 0.0f, 0.0f, 0.0f);
    glBindVertexArray(vao_wire_);
    glDrawArrays(GL_LINES, 0, vertex_count_wire_);
    glBindVertexArray(0);
  };

  void drawWireframe(GLShader& shader) { drawWireOverlay(shader); }

  GLMaterial& material() { return GLMesh::material(); };
  const GLMaterial& material() const { return GLMesh::material(); };

 private:
  std::shared_ptr<MeshL> mesh_;
  GLuint vao_flat_ = 0;
  GLuint vbo_flat_ = 0;
  GLuint vao_wire_ = 0;
  GLuint vbo_wire_ = 0;
  GLint vertex_count_flat_ = 0;
  GLint vertex_count_wire_ = 0;

  void resetVAOVBOHandles() {
    vao_flat_ = vbo_flat_ = 0;
    vao_wire_ = vbo_wire_ = 0;
    vertex_count_flat_ = 0;
    vertex_count_wire_ = 0;
  };

  static Eigen::Vector3f uvPosition(const std::shared_ptr<HalfedgeL>& he) {
    const Eigen::Vector3d& t = he->texcoord()->point();
    return Eigen::Vector3f(static_cast<float>(t.x()), static_cast<float>(t.y()),
                           0.0f);
  }

  void buildBuffers() {
    deleteVAOVBO();
    if (mesh_ == nullptr || mesh_->texcoords().empty()) return;

    std::vector<ParamVertexAttrib> flat_buffer;
    const Eigen::Vector3f normal(0.0f, 0.0f, 1.0f);

    for (const auto& face : mesh_->faces()) {
      std::vector<std::shared_ptr<HalfedgeL>> he_list(face->halfedges().begin(),
                                                      face->halfedges().end());
      if (he_list.size() < 3) continue;

      bool has_uv = true;
      for (const auto& he : he_list) {
        if (!he->isTexcoord()) {
          has_uv = false;
          break;
        }
      }
      if (!has_uv) continue;

      for (size_t i = 1; i + 1 < he_list.size(); ++i) {
        flat_buffer.push_back({uvPosition(he_list[0]), normal});
        flat_buffer.push_back({uvPosition(he_list[i]), normal});
        flat_buffer.push_back({uvPosition(he_list[i + 1]), normal});
      }
    }

    vertex_count_flat_ = static_cast<GLint>(flat_buffer.size());
    if (vertex_count_flat_ > 0) {
      setupVAO(vao_flat_, vbo_flat_, flat_buffer);
    }

    std::vector<float> wire_buffer;
  std::set<std::pair<int, int>> processed_edges;
    for (const auto& face : mesh_->faces()) {
      for (auto& he : face->halfedges()) {
        if (!he->isTexcoord() || !he->next()->isTexcoord()) continue;
        const int id1 = he->vertex()->id();
        const int id2 = he->next()->vertex()->id();
        const int a = std::min(id1, id2);
        const int b = std::max(id1, id2);
        if (processed_edges.count({a, b})) continue;
        processed_edges.insert({a, b});

        const Eigen::Vector3f p1 = uvPosition(he);
        const Eigen::Vector3f p2 = uvPosition(he->next());
        wire_buffer.push_back(p1.x());
        wire_buffer.push_back(p1.y());
        wire_buffer.push_back(p1.z());
        wire_buffer.push_back(p2.x());
        wire_buffer.push_back(p2.y());
        wire_buffer.push_back(p2.z());
      }
    }

    vertex_count_wire_ = static_cast<GLint>(wire_buffer.size() / 3);
    if (vertex_count_wire_ > 0) {
      glGenVertexArrays(1, &vao_wire_);
      glBindVertexArray(vao_wire_);
      glGenBuffers(1, &vbo_wire_);
      glBindBuffer(GL_ARRAY_BUFFER, vbo_wire_);
      glBufferData(GL_ARRAY_BUFFER, wire_buffer.size() * sizeof(float),
                   wire_buffer.data(), GL_STATIC_DRAW);
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
                            (void*)0);
      glBindVertexArray(0);
    }
  }

  void setupVAO(GLuint& vao, GLuint& vbo,
                const std::vector<ParamVertexAttrib>& buffer) {
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, buffer.size() * sizeof(ParamVertexAttrib),
                 buffer.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ParamVertexAttrib),
                          (void*)offsetof(ParamVertexAttrib, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ParamVertexAttrib),
                          (void*)offsetof(ParamVertexAttrib, normal));
    glBindVertexArray(0);
  }
};

#endif
