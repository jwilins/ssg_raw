# 秋霜玉 (Raw Asset Loading)

This fork of nmlgc's Shuusou Gyoku (Seihou 1) source code allows for loading of assets directly from raw files rather than from packed DAT archives. This allows for easy modding of game stage scripts/dialogue, images, etc. Enemy scripts (ECL) and stage scripts/dialogue (SCL) can be modified with the help of tools such as [SSGtk](https://github.com/Clb184/SSGtk).

Instructions for extracting assets so that they can be read by this compile:
1. Download and place rename.bat into your Shuusou Gyoku directory
2. Create folders named "enemy", "graph", "graph2", and "sound" in the same Shuusou Gyoku directory
3. Extract and open the [Brightmoon](https://coolier.net/th_up4/index.php?id=4486) DAT extraction tool in any directory of choice
4. Open each DAT file you created a folder for (ENEMY.DAT, GRAPH.DAT, GRAPH2.DAT, and SOUND.DAT) in Brightmoon, select all of the DATAxxxx files, and extract them into the folder you created with the corresponding name of the DAT
5. Run the rename.bat file that was extracted into the Shuusou Gyoku directory to rename all files in order to be read properly by the compile
6. Renaming is done; feel free to remove rename.bat
7. The game is now ready to be played with the extracted assets! Launch the executable of choice: GIAN07.exe for the updated SDL graphics engine (better for newer systems) or GIAN07D7.exe for the original DirectX 7 graphics engine (better for older systems).

Custom music can be added through BGM packs as the nmlgc compile already supports.

----

Original README by nmlgc below.

----

# 秋霜玉

## Building

This project uses [Tup](https://gittup.org/tup/) as its build system, so install a fitting version for your operating system.

All binaries will be put into the `bin/` subdirectory.

### Windows

Visual Studio ≥2022 is the only compiler supported right now.
However, since IDE integration is horribly broken for both Makefile and directory projects, we strongly recommend literally *anything else* to edit the code.
This repo includes a ready-to-use configuration for Visual Studio Code; If you want to use this editor, make sure to install the default recommended C++ extensions when asked.

To build:

1. Install Visual Studio Community ≥2022, with the *Desktop development for C++* workload.\
   If you haven't already installed the IDE for other projects and don't plan to, you can install only the command-line compilers via the [Build Tools installer](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022).
2. Make sure that `tup.exe` and its DLLs are somewhere in your `PATH`.

3. Open Visual Studio's *x64_x86 Cross Tools Command Prompt*.
4. Navigate to the checkout directory of this repository.
5. Invoke `build.bat` in your way of choice:
   * If you use Visual Studio Code, open the editor from this command-line environment:

     ```batch
     code .
     ```

     Then, you can run the build task with the default `Ctrl-Shift-B` keybinding.

   * Or you can always run `build.bat` directly from this shell.

### Linux

Clang ≥18 is the only compiler supported right now.
Still waiting for GCC to ship [P2465R3 Standard Library Modules](https://wg21.link/P2465R3).

The build is driven by `build.sh`, which sets up the required submodules and environment variables for Tup.
Some libraries are expected to be installed through the distribution's package manager; check the script for details.

Use `install.sh` to copy all files to their standard install locations.

### Filtering build outputs

By default, the process builds both Debug and Release configurations of all binaries.
If you only need a few of them and want to speed up the build process, you can specify any number of target binary filenames as a parameter to the build batch file.

On Windows:

```sh
build.bat bin/GIAN07.exe  # builds only the modern Release binary
build.bat bin/GIAN07d.exe # builds only the modern Debug binary
build.bat                 # builds all binaries, including the vintage ones
```

The Visual Studio Code configuration contains build tasks for all five possibilities.

On Linux:

```sh
./build.sh bin/GIAN07  # builds only the Release binary
./build.sh bin/GIAN07d # builds only the Debug binary
./build.sh             # builds both Debug and Release binaries
```

## Debugging (Windows only)

.PDB files are generated for Debug and Release builds, so you should get symbol support with any Windows debugger.

### Visual Studio Code

Select between Debug and Release modes in the *Run and Debug* menu (`Ctrl-Shift-D` by default), and start debugging with the ▶ button or its keybinding.

### Visual Studio IDE

We don't support it for compilation, but you can still use it for debugging by running

```bat
devenv bin/GIAN07d.exe &::to run the Debug binary
devenv bin/GIAN07.exe  &::to run the Release binary
```

from the *x64_x86 Cross Tools Command Prompt*.
Strangely enough, this yields a superior IntelliSense performance than creating any sort of project. 🤷

----

Original README by pbg below.

----

## これは何？
* 西方プロジェクト第一弾 **秋霜玉** のソースコードです。
* コンパイルできるかもしれませんが, すべてのソースコードが含まれているわけではないのでリンクはできません。
* 画像、音楽、効果音、スクリプト等のリソースは含まれません。


## 参考までに
* 基本、開発当時（2000年前後）のままですが、文字コードを utf-8 に変更し、一部コメント（黒歴史ポエム）は削除してあります。インデント等も当時のままなので、読みにくい箇所があるかもしれません。
* 8bit/16bitカラーの混在、MIDI再生関連、浮動小数点数演算を避ける、あたりが懐かしポイントになるかと思います。
* 8.3形式のファイル名が多いのは、PC-98 時代に書いたコードの一部を流用していたためです。
* リソースのアーカイブ展開に関するコードはもろもろの影響を考え、このリポジトリには含めていません。


## ディレクトリ構成
* /**MAIN** : 秋霜玉WinMainあたり
* /**GIAN07** : 秋霜玉本体
* /**DirectXUTYs** : DirectX, MIDI再生、数学関数等の共通処理
* /**MapEdit2** : マップエディタ
* /**ECLC** : ECL(敵制御用) スクリプトコンパイラ
* /**SCLC** : SCL(敵配置用) スクリプトコンパイラ


## たぶん紛失してしまったソースコード
以下のコードについては、見つかり次第追加するかもしれません。
* リソースのアーカイバ


## ライセンス
* [MIT](LICENSE)
