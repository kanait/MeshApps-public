////////////////////////////////////////////////////////////////////
//
// $Id: LoopSubL.hxx 2025/07/19 18:21:43 kanai Exp 
//
// Copyright (c) 2022-2025 Takashi Kanai
// Released under the MIT license
//
////////////////////////////////////////////////////////////////////

#ifndef _LOOPSUB_HXX
#define _LOOPSUB_HXX 1

#include "envDep.h"
#include "mydef.h"

#include <cmath>
#include <vector>
#include <memory>
//using namespace std;

#include "myEigen.hxx"

#include "EdgeL.hxx"
#include "FaceL.hxx"
#include "MeshL.hxx"
#include "VertexL.hxx"
#include "VertexLCirculator.hxx"
#include "MeshUtiL.hxx"

// masks
#define LOOP_MASK_BETA3 0.1875
#define LOOP_MASK_12 0.5
#define LOOP_MASK_18 0.125
#define LOOP_MASK_38 0.375
#define LOOP_MASK_58 0.625
#define LOOP_MASK_68 0.75

// Loop beta for valence = 4 ... 10
static double loop_beta[7] = {
  0.121094,
  0.0840932,
  0.0625,
  0.0490249,
  0.0400678,
  0.033785,
  0.0291778
};


class LoopSub {
 public:
  LoopSub() : mesh_(nullptr), submesh_(nullptr){};
  LoopSub(std::shared_ptr<MeshL> mesh) : submesh_(nullptr) { setMesh(mesh); };
  LoopSub(std::shared_ptr<MeshL> mesh, std::shared_ptr<MeshL> submesh) {
    setMesh(mesh);
    setSubMesh(submesh);
  };
  ~LoopSub(){};

  void setMesh(std::shared_ptr<MeshL> mesh) { mesh_ = mesh; };
  void setSubMesh(std::shared_ptr<MeshL> mesh) { submesh_ = mesh; };
  MeshL& submesh() const { return *submesh_; };

  bool emptyMesh() const { return (mesh_ != nullptr) ? false : true; };
  bool emptySubMesh() const { return (submesh_ != nullptr) ? false : true; };

  void apply() {
    if (init() == false) return;
    setSplit();
    setStencil();
    submesh_->calcAllFaceNormals();
    std::cout << "loop subdiv.: done. v " << submesh_->vertices_size()
              << " f " << submesh_->faces_size() << std::endl;
    clear();
  };

  bool init() {
    if (emptyMesh()) return false;
    if (emptySubMesh()) return false;

    // check for rectangle faces
    for (auto& fc : mesh_->faces()) {
      if (fc->size() != TRIANGLE) {
        std::cerr << "Error: A non-triangle face is included. " << std::endl;
        return false;
      }
    }

    // don't delete edges
    mesh_->createConnectivity(false);

    return true;
  };

  void clear() {
    even_.clear();
    odd_.clear();
  };

  // ここに分割処理のコードを追加してください．
  void setSplit() {
    Eigen::Vector3d p = Eigen::Vector3d::Zero();

    // submesh の even vertex を生成 (mesh の頂点数)
    even_.resize(mesh_->vertices_size());
    for (int i = 0; i < mesh_->vertices_size(); ++i)
      even_[i] = submesh_->addVertex(p);

    // submesh の odd vertex を作成 (mesh のエッジの数)
    odd_.resize(mesh_->edges().size());
    for (int i = 0; i < mesh_->edges().size(); ++i)
      odd_[i] = submesh_->addVertex(p);

    // submesh faces
    for (auto& fc : mesh_->faces()) {
      //
      // ここに 4-to-1 分割の新しい4つの面を submesh に作成するコードを追加
      //

    }
  };

  // ここに位置計算処理のコードを追加してください．
  void setStencil() {
    for (auto& vt : mesh_->vertices()) {
      Eigen::Vector3d p;
      //
      // ここに even vertex の頂点位置の計算コードを追加
      //

      even_[vt->id()]->setPoint(p);
    }

    // odd vertex points
    for (auto ed : mesh_->edges()) {
      Eigen::Vector3d p;
      //
      // ここに odd vertex の頂点位置の計算コードを追加
      //

      odd_[ed->id()]->setPoint(p);

    }
  };

  double beta(int valence) {
    if (valence == 3)
      return LOOP_MASK_BETA3;
    else if (valence > 10)
      return calcBeta(valence);
    else
      return loop_beta[valence-4];
  };

  double calcBeta(int valence) {
    double dval = (double)valence;
    double d = LOOP_MASK_38 + std::cos(2.0 * M_PI / dval) / 4.0;
    double beta = (LOOP_MASK_58 - d * d) / dval;
    return beta;
  };

private:
  // original mesh
  std::shared_ptr<MeshL> mesh_;

  // subdivided mesh
  std::shared_ptr<MeshL> submesh_;

  // vertices pointer
  std::vector<std::shared_ptr<VertexL> > even_;  // even vertex
  std::vector<std::shared_ptr<VertexL> > odd_;   // odd vertex
};

#endif  // _LOOPSUB_HXX
