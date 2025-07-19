////////////////////////////////////////////////////////////////////
//
// $Id: KdTree.hxx 2025/07/19 18:39:44 kanai Exp 
//
// Copyright (c) 2024-2025 Takashi Kanai
// Released under the MIT license
//
////////////////////////////////////////////////////////////////////

#ifndef __KDTREE_HXX__
#define __KDTREE_HXX__ 1

#include <vector>
#include <queue>
#include <cmath>
#include <limits>
#include <numeric>
//using namespace std;

#include "myEigen.hxx"

//
// kD-Tree のノード
// - axis_ 上の中間点 points[idx_] を持つ
// - 子ノードのポインタを持つ
//
class KdNode {

public:

  KdNode() : idx_(-1), axis_(-1) { child_[0] = child_[1] = nullptr; };
  KdNode(int idx, int axis) : idx_(idx), axis_(axis) { child_[0] = child_[1] = nullptr; };
  ~KdNode(){};

  int idx() const { return idx_; };
  int axis() const { return axis_; };

  KdNode* child(int i) { return child_[i]; };
  void setChild(int i, KdNode* node) { child_[i] = node; };

private:

  int idx_;          // 点のインデックス
  KdNode* child_[2]; // 子ノードのポインタ
  int axis_;         // N次元の軸 (0 ... N-1)
};

//
// N次元 kD-Tree クラス
// - N はテンプレートで指定
// - root_ を根とする KdNode の二分木構造
// - points_ : N次元点を配列 (vector) で持つ
//
template<int N>
class KdTree {

public:

  KdTree() : root_(nullptr) {};
  ~KdTree(){ clear(); };

  std::vector<Eigen::Vector<double,N> >& points() { return points_; };
  KdNode* root() { return root_; };

  // kD-Tree の構築
  void construct(const std::vector<Eigen::Vector<double,N> >& points) {
    points_ = points;  // 点データをコピー

    ///////// ここに構築用のコードを書く ///////////


    ///////////////////////////////////////////
  };

//////////// ここに構築再帰用のコードを書く //////////////


////////////////////////////////////////////////////

  // 最近傍点の探索: 最近傍点の点のインデックスを返す
  // q: クエリ点
  // min_dis: 最短距離
  int nnSearch(const Eigen::Vector<double,N>& query, double* min_dis = nullptr) {

    int index; // 最近傍点のインデックス

    //////// ここに最近傍点探索のコードを書く ////////


    ///////////////////////////////////////////
    return index;
  };

///////// ここに最近傍点探索再帰用のコードを書く ///////////


////////////////////////////////////////////////////
  
  // K近傍点の探索: k個の近傍点のインデックス列を返す（インデックス列は距離の近い順に並んでいる必要がある）
  // q: クエリ点
  // k: 近傍点の数
  std::vector<int> knnSearch(const Eigen::Vector<double,N>& query, int k) {

    std::vector<int> indices; // K個の近傍点のインデックス
    
    ////////// K近傍点探索のコードを書く ///////////


    ///////////////////////////////////////////
    return indices;
  };

///////// ここにK近傍点探索再帰用のコードを書く ///////////


////////////////////////////////////////////////////

  // 半径探索: 半径r内の近傍点のインデックス列を返す
  // q: クエリ点
  // r: 半径
  std::vector<int> radiusSearch(const Eigen::Vector<double,N>& q, double r) {

    std::vector<int> indices; // 半径内の近傍点のインデックス

    /////////// 半径探索のコードを書く ////////////


    ///////////////////////////////////////////
    return indices;
  };

private:

  // kD-Tree 削除
  void clear() {
    clearChild(root_);
    root_ = nullptr;
    points_.clear();
  };

  // kD-Tree 削除 再帰関数
  void clearChild(KdNode* node) {
    if (node == nullptr) return;
    clearChild(node->child(0));
    clearChild(node->child(1));
    delete node;
  };

  // 2点間の距離
  double distance(const Eigen::Vector<double,N>& p, const Eigen::Vector<double,N>& q) {
    return (p - q).norm();
  };

  //
  // メンバ変数
  //

  KdNode* root_; // KdNode 根ノード
  std::vector<Eigen::Vector<double,N> > points_; // 点の vector 配列

};

#endif // __KDTREE_HXX__
