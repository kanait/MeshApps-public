////////////////////////////////////////////////////////////////////
//
// Tutte UV parameterization for MeshL.
//
// Copyright (c) 2026 Takashi Kanai
// Released under the MIT license
//
////////////////////////////////////////////////////////////////////

#ifndef _TUTTEPARAM_HXX
#define _TUTTEPARAM_HXX 1

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <vector>

#include "myEigen.hxx"

#include "BLoopL.hxx"
#include "FaceL.hxx"
#include "HalfedgeL.hxx"
#include "MeshL.hxx"
#include "MeshUtiL.hxx"
#include "TexcoordL.hxx"
#include "VertexLCirculator.hxx"
#include "mydef.h"

// TutteParam は，境界を正方形に固定し，内部頂点を隣接頂点の
// 単純平均として求める UV パラメータ化である．
//
// 処理の大きな流れ:
// 1. 境界ループを順序付き頂点列として取り出す
// 2. 境界ループ上で，3D 境界長がほぼ 4 等分になる 4 角を選ぶ
// 3. 4 角を正方形の角へ対応させ，境界頂点を各辺上へ配置する
// 4. 内部頂点について Tutte の平均値方程式を作る
// 5. u と v をそれぞれ Eigen の疎行列ソルバで解く
// 6. 求めた UV を TexcoordL として MeshL に割り当てる
//
// Tutte の内部方程式は，各内部頂点 i について
//
//   p_i = sum_j lambda_ij p_j
//
// である．ここで p_i = (u_i, v_i)，j は i の隣接頂点である．
// オリジナルの Tutte mapping では，各隣接頂点に unit mass を置くため，
// lambda_ij = 1 / deg(i) という単純平均になる．
class TutteParam {
 public:
  TutteParam() : mesh_(nullptr) {};
  explicit TutteParam(std::shared_ptr<MeshL> mesh) : mesh_(mesh) {};

  void setMesh(std::shared_ptr<MeshL> mesh) { mesh_ = mesh; };

  bool apply() {
    if (mesh_ == nullptr) {
      std::cerr << "TutteParam: mesh is null." << std::endl;
      return false;
    }

    // 境界判定，境界ループ作成，隣接頂点巡回には halfedge の mate や
    // next が必要になるため，最初に接続関係を作成する．
    mesh_->createConnectivity(true);
    indexVertices();

    // boundary は境界ループの順序で並んだ頂点列である．
    // collectBoundaryVertices() の中で，境界頂点には is_boundary_ の印も付ける．
    // この順序が，後で正方形の 4 辺へ境界を貼り付けるときに必要になる．
    std::vector<std::shared_ptr<VertexL>> boundary;
    if (!collectBoundaryVertices(boundary)) {
      std::cerr << "TutteParam: failed to collect boundary loop." << std::endl;
      return false;
    }

    // corners には boundary 配列上のインデックスを入れる．
    // つまり corners[k] は「k 番目の角の頂点番号」ではなく，
    // 「boundary の中で何番目が角か」を表す．
    std::array<int, 4> corners;
    if (!chooseBoundaryCorners(boundary, corners)) {
      std::cerr << "TutteParam: failed to choose four boundary corners."
                << std::endl;
      return false;
    }

    if (!mapBoundaryToSquare(boundary, corners)) {
      std::cerr << "TutteParam: failed to map boundary to square." << std::endl;
      return false;
    }

    if (!solveInterior()) {
      std::cerr << "TutteParam: failed to solve linear system." << std::endl;
      return false;
    }

    assignTexcoords();

    std::cout << "tutteparam: done. v " << mesh_->vertices_size() << " t "
              << mesh_->texcoords().size() << std::endl;
    return true;
  };

 private:
  std::shared_ptr<MeshL> mesh_;

  // Eigen の行列では頂点を 0, 1, ..., n-1 の連番で扱う．
  // MeshL の VertexL は shared_ptr なので，双方向の対応表を持っておく．
  std::vector<std::shared_ptr<VertexL>> vtx_order_;
  std::map<std::shared_ptr<VertexL>, int> vtx_index_;

  // 求解結果の UV 座標．uv_u_[i], uv_v_[i] が頂点 i の 2D 座標になる．
  std::vector<double> uv_u_;
  std::vector<double> uv_v_;

  // 境界頂点かどうかを頂点インデックスで引くための配列．
  // 境界頂点は Dirichlet 条件として固定し，内部頂点だけを平均値方程式で解く．
  std::vector<bool> is_boundary_;

  void indexVertices() {
    // MeshL の頂点リストの走査順で，連立方程式用の頂点番号を付ける．
    // この番号は SparseMatrix の行番号・列番号，および uv_u_/uv_v_ の
    // 添字として使う．
    vtx_order_.clear();
    vtx_index_.clear();
    int i = 0;
    for (auto& vt : mesh_->vertices()) {
      vtx_index_[vt] = i++;
      vtx_order_.push_back(vt);
    }

    const int n = static_cast<int>(vtx_order_.size());
    uv_u_.assign(n, 0.0);
    uv_v_.assign(n, 0.0);
    is_boundary_.assign(n, false);
  }

  bool collectBoundaryVertices(std::vector<std::shared_ptr<VertexL>>& boundary) {
    boundary.clear();

    // OBJ から読んだ直後は BLoop が無いことがある．その場合は，
    // まず境界上の頂点を 1 つ探し，MeshL 側の createBLoop() に
    // 境界エッジを一周させる．
    if (mesh_->emptyBLoop()) {
      auto start = mesh_->findBoundaryVertex();
      if (start == nullptr) return false;
      mesh_->createBLoop(start);
    }

    auto bloop = mesh_->bloop();
    if (bloop == nullptr) return false;

    // bloop->vertices() は境界を一周する順序付き頂点列である．
    // Tutte Mapping では境界を正方形に貼るため，単なる集合ではなく
    // この順序が重要になる．
    boundary = bloop->vertices();

    // 四辺形境界へ写すので，少なくとも 4 頂点が必要である．
    if (boundary.size() < 4) return false;

    // ここで境界頂点に印を付けておく．
    // 後の solveInterior() はこの印を見て，
    // 境界頂点なら Dirichlet 条件，内部頂点なら平均値条件を入れる．
    for (auto& vt : boundary) {
      is_boundary_[vtx_index_.at(vt)] = true;
    }

    return true;
  }

  std::vector<double> boundaryEdgeLengths(
      const std::vector<std::shared_ptr<VertexL>>& boundary) const {
    const int nb = static_cast<int>(boundary.size());
    std::vector<double> edge_len(nb, 0.0);

    // edge_len[i] は boundary[i] から boundary[i+1] への 3D 境界辺長．
    // 最後の頂点は最初の頂点へ戻るので，添字は modulo nb で扱う．
    for (int i = 0; i < nb; ++i) {
      const auto& p0 = boundary[i]->point();
      const auto& p1 = boundary[(i + 1) % nb]->point();
      edge_len[i] = (p1 - p0).norm();
    }
    return edge_len;
  }

  bool chooseBoundaryCorners(const std::vector<std::shared_ptr<VertexL>>& boundary,
                             std::array<int, 4>& corners ) const {
    const int nb = static_cast<int>(boundary.size());
    if (nb < 4) return false;

    // 境界を 3D 上の長さで 4 等分する．
    // cumulative[i] は，boundary[0] から boundary[i] まで境界上を
    // 歩いたときの累積長である．
    const std::vector<double> edge_len = boundaryEdgeLengths(boundary);
    std::vector<double> cumulative(nb + 1, 0.0);
    for (int i = 0; i < nb; ++i) {
      cumulative[i + 1] = cumulative[i] + edge_len[i];
    }

    const double perimeter = cumulative[nb];
    if (perimeter < 1.0e-12) return false;

    // 0 番目の角は boundary[0] に固定する．残り 3 角は，
    // 境界周長の 1/4, 2/4, 3/4 に最も近い頂点を選ぶ．
    // こうすると，正方形の各辺に対応する 3D 境界長がほぼ等しくなる．
    corners[0] = 0;
    for (int c = 1; c < 4; ++c) {
      const double target = perimeter * static_cast<double>(c) / 4.0;

      // 角の順序が崩れたり，同じ頂点が複数の角に選ばれたりしないように，
      // 前の角より後ろ，かつ残りの角を置く余地がある範囲だけを調べる．
      const int min_idx = corners[c - 1] + 1;
      const int max_idx = nb - (4 - c);
      if (min_idx > max_idx) return false;

      int best = min_idx;
      double best_diff = std::numeric_limits<double>::max();
      for (int i = min_idx; i <= max_idx; ++i) {
        const double diff = std::fabs(cumulative[i] - target);
        if (diff < best_diff) {
          best_diff = diff;
          best = i;
        }
      }
      corners[c] = best;
    }

    // corners は boundary 配列内で単調増加している必要がある．
    // これが満たされると，4 つの角が境界ループ上に正しい順序で並ぶ．
    return corners[0] < corners[1] && corners[1] < corners[2] &&
           corners[2] < corners[3] && corners[3] < nb;
  }

  // 境界頂点の固定 UV を正方形上に配置する
  bool mapBoundaryToSquare(
      const std::vector<std::shared_ptr<VertexL>>& boundary,
      const std::array<int, 4>& corners) {
    // ここから
    //
    // 入力:
    //   boundary:
    //     境界ループ上の頂点を，境界を一周する順序で並べた配列．
    //     boundary[k] と boundary[(k+1)%nb] が境界上で隣り合う．
    //
    //   corners:
    //     正方形の 4 角に対応させる境界頂点の位置．
    //     値は頂点番号ではなく，boundary 配列上のインデックスである．
    //     corners[0], corners[1], corners[2], corners[3] は，
    //     境界ループ上でこの順に並んでいることを仮定する．
    //
    // 出力:
    //   uv_u_, uv_v_:
    //     境界頂点の固定 UV 座標をここに書き込む．
    //     頂点 boundary[k] の線形方程式用インデックス vid は
    //     vtx_index_.at(boundary[k]) で得られる．
    //     その頂点の固定 UV は uv_u_[vid], uv_v_[vid] に代入する．
    //
    //   戻り値:
    //     正しく境界を正方形へ配置できたら true を返す．
    //     境界頂点数が足りない，角の並びが不正，辺長が 0 に近いなど，
    //     正しい固定 UV を作れない場合は false を返す．
    //
    // 実装方針:
    //   1. boundary の頂点数 nb が 4 以上であることを確認する．
    //   2. 正方形の 4 角を用意する．
    //        side 0: (0,0) -> (1,0)
    //        side 1: (1,0) -> (1,1)
    //        side 2: (1,1) -> (0,1)
    //        side 3: (0,1) -> (0,0)
    //   3. boundaryEdgeLengths(boundary) を使い，
    //      境界上の各辺の 3D 長さ edge_len[k] を得る．
    //   4. side = 0,1,2,3 の順に，4 つの境界区間を処理する．
    //      side 番目の区間は corners[side] から
    //      corners[(side+1)%4] までである．
    //   5. 最後の区間では，corners[3] から boundary の末尾を通り，
    //      boundary の先頭へ戻って corners[0] に向かう．
    //      stop <= start の場合は stop に nb を足し，
    //      start <= k < stop の半開区間として扱うと実装しやすい．
    //   6. その区間の 3D 境界長 side_len を求める．
    //      side_len が 0 に近い場合は false を返す．
    //   7. 区間の始点から現在の境界頂点までの累積距離 dist を使い，
    //        t = dist / side_len
    //      を求める．t は正方形の辺上での補間パラメータである．
    //   8. 正方形上の始点を q0，終点を q1 とすると，
    //        uv = (1-t) q0 + t q1
    //      により，現在の境界頂点の固定 UV が得られる．
    //   9. 得られた uv を uv_u_[vid], uv_v_[vid] に書き込む．
    //      ここでは is_boundary_ は変更しない．境界頂点の印は
    //      collectBoundaryVertices() ですでに付けてある．
    //  10. 4 辺すべての境界頂点へ固定 UV を入れられたら true を返す．

    // ここまで
    return true;
  }

  bool solveInterior() {
    const int n = static_cast<int>(vtx_order_.size());
    if (n == 0) return false;

    // Tutte Mapping では u と v は結合しない．
    // したがって同じ n x n 行列 A を使って，
    //
    //   A u = b_u
    //   A v = b_v
    //
    // を別々に解く．Triplet は疎行列の非ゼロ要素を一時的に持つ形式である．
    std::vector<Eigen::Triplet<double>> triplets;
    triplets.reserve(static_cast<size_t>(n) * 8);
    Eigen::VectorXd bu(n), bv(n);
    bu.setZero();
    bv.setZero();

    // ここから
    //
    // 入力:
    //   vtx_order_:
    //     線形方程式で使う頂点配列．頂点 i は vtx_order_[i] で得られる．
    //
    //   vtx_index_:
    //     VertexL から線形方程式用の頂点番号を引く表．
    //     隣接頂点 nvt の列番号 j は vtx_index_.at(nvt) で得られる．
    //
    //   is_boundary_:
    //     頂点 i が境界頂点かどうかを表す配列．
    //
    //   uv_u_, uv_v_:
    //     境界頂点については，mapBoundaryToSquare() で固定 UV が入っている．
    //
    // 出力:
    //   triplets:
    //     疎行列 A の非ゼロ係数を追加する．
    //     triplets.emplace_back(row, col, value) は，
    //       A(row, col) += value
    //     という係数を後で SparseMatrix に渡すための記録である．
    //
    //   bu, bv:
    //     A u = bu, A v = bv の右辺ベクトル．
    //     境界頂点の行には固定 UV を入れ，内部頂点の行は 0 のままにする．
    //
    // 実装方針:
    //   1. すべての頂点番号 i = 0, ..., n-1 を順に処理する．
    //
    //   2. is_boundary_[i] が true の場合:
    //        境界頂点なので Dirichlet 条件を入れる．
    //          u_i = uv_u_[i]
    //          v_i = uv_v_[i]
    //        行列 A の i 行は単位行にする．
    //          A(i,i) = 1
    //          A(i,j) = 0  (j != i)
    //        triplets には非ゼロの A(i,i) だけを追加すればよい．
    //        右辺 bu[i], bv[i] には固定済みの uv_u_[i], uv_v_[i] を入れる．
    //
    //   3. is_boundary_[i] が false の場合:
    //        内部頂点なので Tutte の平均値条件を入れる．
    //          p_i = (1 / d_i) * sum_{j in N(i)} p_j
    //        VertexLCirculator を使って vtx_order_[i] の隣接頂点を集める．
    //        隣接頂点数 d_i が 0 の場合は false を返す．
    //
    //   4. 内部頂点の式を左辺へ移すと，
    //          p_i - sum_j lambda_ij p_j = 0
    //        であり，オリジナルの Tutte mapping では
    //          lambda_ij = 1 / d_i
    //        である．したがって i 行の係数は，
    //          A(i,i) = 1
    //          A(i,j) = -lambda_ij  for each neighbor j
    //        となる．
    //
    //   5. u と v で行列 A は同じなので，triplets は 1 つだけ作る．
    //      内部頂点の右辺は bu[i] = 0, bv[i] = 0 のままでよい．
    //
    //   6. すべての行について必要な triplet と右辺を設定できたら，
    //      この空欄の後にある setFromTriplets() が SparseMatrix A を作る．

    // ここまで

    return false; // 上記の空欄を埋めたら，この一文をコメントにする

    // Triplet のリストから Eigen の SparseMatrix を作る．
    Eigen::SparseMatrix<double> A(n, n);
    A.setFromTriplets(triplets.begin(), triplets.end());
    A.makeCompressed();

    // 正方形境界の Dirichlet 条件が入っているため，Tutte の線形系は
    // 通常は非特異になる．ここでは直接法 SparseLU で解く．
    Eigen::SparseLU<Eigen::SparseMatrix<double>, Eigen::COLAMDOrdering<int>>
        solver;
    solver.analyzePattern(A);
    solver.factorize(A);
    if (solver.info() != Eigen::Success) return false;

    const Eigen::VectorXd u = solver.solve(bu);
    if (solver.info() != Eigen::Success || !u.allFinite()) return false;

    const Eigen::VectorXd v = solver.solve(bv);
    if (solver.info() != Eigen::Success || !v.allFinite()) return false;

    for (int i = 0; i < n; ++i) {
      uv_u_[i] = u[i];
      uv_v_[i] = v[i];
    }
    return true;
  }

  void assignTexcoords() {
    // 既存の texcoord が入力 OBJ に含まれている場合でも，
    // 今回計算した Tutte Mapping の UV に置き換える．
    mesh_->deleteAllTexcoords();

    // MeshL では texcoord は VertexL ではなく HalfedgeL から参照される．
    // まず各頂点に対応する TexcoordL を 1 つ作っておき，
    // 後で face の halfedge に割り当てる．
    std::vector<std::shared_ptr<TexcoordL>> texcoords(vtx_order_.size());
    for (size_t i = 0; i < vtx_order_.size(); ++i) {
      Eigen::Vector3d uv(uv_u_[i], uv_v_[i], 0.0);
      texcoords[i] = mesh_->addTexcoord(uv);
    }

    // 各 halfedge の始点頂点に対応する texcoord を割り当てる．
    // 表示側の GLMeshL / GLParamMeshL は halfedge->texcoord() を見るので，
    // ここを忘れると UV を計算していても表示や OBJ 出力に反映されない．
    for (auto& fc : mesh_->faces()) {
      for (auto& he : fc->halfedges()) {
        const int i = vtx_index_.at(he->vertex());
        he->setTexcoord(texcoords[i]);
      }
    }
  }
};

#endif
