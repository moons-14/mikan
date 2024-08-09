# mikan os

Re:ゼロから始めるOS自作入門

### カーネルのビルド
```bash
cd kernel
source $HOME/osbook/devenv/buildenv.sh
clang++ $CPPFLAGS -O2 -Wall -g --target=x86_64-elf -ffreestanding -mno-red-zone -fno-exceptions -fno-rtti -std=c++17 -c main.cpp
ld.lld $LDFLAGS --entry KernelMain -z norelro --image-base 0x100000 --static -o kernel.elf main.o
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