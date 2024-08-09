# mikan os

Re:ゼロから始めるOS自作入門

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
source edksetup.sh
build
```

### シミュレータで起動
```bash
$HOME/osbook/devenv/run_qemu.sh $HOME/edk2/Build/MikanLoaderX64/DEBUG_CLANG38/X64/Loader.efi $HOME/mikan/kernel/kernel.elf
```