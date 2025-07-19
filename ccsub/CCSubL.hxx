////////////////////////////////////////////////////////////////////
//
// $Id: CCSubL.hxx 2025/07/19 18:24:53 kanai Exp 
//
// Copyright (c) 2022-2025 Takashi Kanai
// Released under the MIT license
//
////////////////////////////////////////////////////////////////////

#ifndef _CCSUBL_HXX
#define _CCSUBL_HXX 1

#include "envDep.h"
#include "mydef.h"

#include <cmath>
#include <vector>
#include <memory>
//using namespace std;

#include "VertexL.hxx"
#include "EdgeL.hxx"
#include "FaceL.hxx"
#include "MeshL.hxx"
#include "VertexLCirculator.hxx"
#include "MeshUtiL.hxx"

// masks

#define CC_MASK_EVEN_VV 1.5   // connected vertex
#define CC_MASK_EVEN_VA .25   // diagonal vertex
#define CC_MASK_EVEN_VC 1.75  // center vertex

#define CC_MASK_EDGE_VA .0625
#define CC_MASK_EDGE_VV .375

#define CC_MASK_FACE_VV .25

class CCSubL {
  // original mesh
  MeshL* mesh_;

  // subdivided mesh
  MeshL* submesh_;

  // vertices pointer
  std::vector<std::shared_ptr<VertexL>> even_;  // even vertex
  std::vector<std::shared_ptr<VertexL>> edvt_;  // edge vertex
  std::vector<std::shared_ptr<VertexL>> fcvt_;  // face vertex

 public:
  CCSubL() : mesh_(nullptr), submesh_(nullptr){};
  CCSubL(MeshL& mesh) : submesh_(nullptr) { setMesh(mesh); };
  CCSubL(MeshL& mesh, MeshL& submesh) {
    setMesh(mesh);
    setSubMesh(submesh);
  };
  ~CCSubL(){};

  void setMesh(MeshL& mesh) { mesh_ = &mesh; };
  void setSubMesh(MeshL& mesh) { submesh_ = &mesh; };

  bool emptyMesh() const { return (mesh_ != nullptr) ? false : true; };
  bool emptySubMesh() const { return (submesh_ != nullptr) ? false : true; };

  void apply() {
    if (init() == false) return;
    setSplit();
    setStencil();
    submesh_->calcAllFaceNormals();
    std::cout << "cc subdiv.: done. v " << submesh_->vertices_size()
              << " f " << submesh_->faces_size() << std::endl;
  };

  // bool init();
  bool init() {
    if (emptyMesh()) return false;
    if (emptySubMesh()) return false;

    // check for rectangle faces
    for ( auto& fc : mesh_->faces() ) {
      if (fc->size() != RECTANGLE) {
        std::cerr << "Error: A non-rectangle face is included. " << std::endl;
        return false;
      }
    }

    // don't delete edges
    mesh_->createConnectivity(false);

    return true;
  };

  void clear() {
    even_.clear();
    edvt_.clear();
    fcvt_.clear();
  };

  // ここに分割処理のコードを追加してください．
  void setSplit() {
    Eigen::Vector3d p = Eigen::Vector3d::Zero();

    // submesh の even vertex を生成 (mesh の頂点の数)
    even_.resize(mesh_->vertices().size());
    for (int i = 0; i < mesh_->vertices().size(); ++i)
      even_[i] = submesh_->addVertex(p);

    // submesh の edge vertex を生成 (mesh のエッジの数)
    edvt_.resize(mesh_->edges().size());
    for (int i = 0; i < mesh_->edges().size(); ++i)
      edvt_[i] = submesh_->addVertex(p);

    // submesh の face vertex を生成 (mesh の面の数)
    fcvt_.resize(mesh_->faces().size());
    for (int i = 0; i < mesh_->faces().size(); ++i)
      fcvt_[i] = submesh_->addVertex(p);

    for ( auto& fc : mesh_->faces() ) {
      //
      // ここに 4-to-1 分割の新しい4つの面を submesh に作成するコードを追加
      //

    }
  };

  // ここに位置計算処理のコードを追加してください．
  void setStencil() {

    // even vertex point
    for ( auto& vt : mesh_->vertices() ) {
      Eigen::Vector3d p;
      //
      // ここに even vertex の頂点位置 p の計算コードを追加
      //

      even_[vt->id()]->setPoint(p);
    }

    // edge vertex point
    for ( auto& ed : mesh_->edges() ) {
      Eigen::Vector3d p;
      //
      // ここに edge vertex の頂点位置 p の計算コードを追加
      //

      edvt_[ed->id()]->setPoint(p);
    }

    // face vertex point
    for ( auto& fc : mesh_->faces() ) {
      Eigen::Vector3d p;
      //
      // ここに face vertex の頂点位置 p の計算コードを追加
      //

      fcvt_[fc->id()]->setPoint(p);
    }
  };
};

#endif  // _CCSUBL_HXX
