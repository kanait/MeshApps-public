# MeshApps-public: プログラミング課題用レポジトリ

まずこのレポジトリをクローンします．必ず --recursive をつけてください．
```
% git clone --recursive https://github.com/kanait/MeshApps-public.git
```

# コンパイル

コンパイル方法はPCの種類によって異なります．

## macOS

ソースコードのコンパイルには g++, make, GLFW, cmake, eigen が必要です．
g++, make は xcode command line tools が必要となります．
GLFW, cmake, eigen は，brew を使ってあらかじめインストールしてください．
```
% brew install glfw cmake eigen
```
展開フォルダの中で以下を実行してください．
```
% cd MeshApps-public
% mkdir build
% cd build
% cmake -DCMAKE_BUILD_TYPE=Release ..
% make -j$(nproc)
```

## ubuntu Linux / WSL2 (Windows)

ほぼ macOS でのコンパイルと同じですが，apt を使ったソフトウェアのインストール方法が異なります．
g++, make は，build-essential のインストールで入ります．
```
sudo apt install build-essential
sudo apt install cmake libglfw3-dev libeigen3-dev
```
ビルド方法は macOS と同じですので参考にしてください．

## Visual Studio (Windows)

Windows でのコンパイルはなるべく WSL2 で行なってください．WSL2 がなんらかの事情で入れられない時のみ，
Visual Studio, Cmake for Windows, vcpkg を使ったコンパイルを試してみてください．

詳しいインストール方法は教員にお尋ねください．

# 実行

コンパイルが終了したら build フォルダで以下を実行してください．

### loopsub

```
% ./loopsub ../common/common/bunnynh_sub500.obj
```
bunnynh_sub500 が表示されたら正常に実行できています．

### ccsub

```
% ./ccsub ../common/common/data/41.obj
```
41 が表示されたら正常に実行できています．

### smooth

```
% ./smooth ../common/common/data/fandisk.obj
```

fandisk が表示されたら正常に実行できています．

### octree

```
% ./octree ../common/common/data/bunny.obj
```
bunny が表示されたら正常に実行できています．

### kdtree2d

```
% ./kdtree2d
```
2Dの画面に赤い点群が表示されたら正常に実行できています．

## data

common/common/data データの中に以下のファイルが含まれています．

- bunnynh_sub500.obj, venus_sub1000.obj ... Loop 細分割用データ

- 41.obj, oloid64_quad.obj, spot_quadrangualted.obj ... Catmull-Clark 細分割用データ

- fandisk.obj, mechpart.obj ... スムースシェーディング用データ

- bunny.obj ... 八分木用データ
