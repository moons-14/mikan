# mikan os

Re:ゼロから始めるOS自作入門



参考・引用: [uchan-nos/mikanos](https://github.com/uchan-nos/mikanos)

## ビルドの方法
https://github.com/uchan-nos/mikanos-build?tab=readme-ov-file
こちらのリポジトリの環境構築を参考にビルド環境を作る
開発中の環境はubuntu 18.04, wsl

### カーネルのビルド
```bash
cd kernel
source $HOME/osbook/devenv/buildenv.sh
make
```
./kernel/kernel.elfに生成されます

### ブートローダーのビルド
```bash
cd $HOME/edk2
ln -s $HOME/mikan/MikanLoaderPkg ./
ln -s $HOME/mikan/kernel/frame_buffer_config.hpp $HOME/mikan/MikanLoaderPkg/frame_buffer_config.hpp
ln -s $HOME/mikan/kernel/elf.hpp $HOME/mikan/MikanLoaderPkg/elf.hpp
source edksetup.sh

build
```

### シミュレータで起動
```bash
$HOME/osbook/devenv/run_qemu.sh $HOME/edk2/Build/MikanLoaderX64/DEBUG_CLANG38/X64/Loader.efi $HOME/mikan/kernel/kernel.elf
```