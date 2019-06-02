### armv6m-sim
Simple instruction set simulator for ARMv6-M (i.e Cortex M0)

Github:   [http://github.com/ultraembedded/armv6m-sim](https://github.com/ultraembedded/armv6m-sim)

#### Intro
A very simple ARMv6-M instruction set simulator.

#### Building

Dependencies;
* gcc
* make
* libelf
* libbfd

To install the dependencies on Linux Ubuntu/Mint;
```
sudo apt-get install libelf-dev binutils-dev
```

To build the executable, type:
```
make
````

#### Usage: Standalone Mode
```
# 0xNNNN is the entry point for the ELF (vector table)
armv6m-sim -f your_elf.elf -X 0xNNNN
```

#### Usage: GDB Mode
```
# Start simulator in GDB mode
armv6m-sim -f your_elf.elf -X 0xNNNN -g & 

# Start GDB and execute program (stopping main)
arm-none-eabi-gdb your_elf.elf
> target remote localhost:3333
> break main
> cont
```
