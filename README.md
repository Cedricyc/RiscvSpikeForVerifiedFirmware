VFSpike: A Modified Spike for Verified Firmware
==================================

#### Email: yucheng_chen@qq.com

## Intro 

Modifications of VFSpike are based on https://github.com/riscv/riscv-isa-sim/tree/master/riscv to make a customized spike to run the Verified Firmware. Please view basic info about spike at that github site README.md. This README.md only contains info relevant to modifications made to the original spike. 

## Motivation

The original spike can't meet the demand to run verified firmware commands:

```shell
./spike --pc=0x93000000 -m0x90000000:0x10000000,0x80000000:0x20000000 ++/home/yuchengchen/vf/device/sw/bbl/build/bbl0 ++load_rom ++chrome_rom ++load_files=/home/yuchengchen/vf/keys/root_key.vbpubk__0x95600000,/home/yuchengchen/vf/keys/firmware.vblock__0x95601000,/home/yuchengchen/vf/device/sw/bbl/build/bbl1__0x95603000,/home/yuchengchen/vf/keys/firmware_v2.vblock__0x95701000,/home/yuchengchen/vf/device/sw/bbl/build/bbl1__0x95703000,/home/yuchengchen/vf/device/sw/lib/gptk.bin__0x90000000  ++chip_config=nc_1__f_1000__xlen_64__isa_rv64imafdc  ++shift_file= ++mems=0x90000000:0x10000000,0x80000000:0x20000000 ++load_flash=0x93000000
```

Specifically speaking, verified firmware **demand** following  3 customized functionality to run command above:

| Demand                                                       | Relevant Module | Dependency                                 |
| ------------------------------------------------------------ | --------------- | ------------------------------------------ |
| (1). Write Verified File (i.e. root_key.vbpubk) to Certain Memory Address | htif_t          | Memory Interface(Mem IF), HTIF Parser      |
| (2). Customized dtb                                          | sim_t, htif_t   | Chip Config Parser, Processor Info, Mem IF |
| (3). Customized ROM                                          | sim_t,htif_t    | Customized dtb, start_pc, rstvec           |

We can decouple the original spike modules, then regroup them with our new modules. To implement the VFSpike, we use 11 modules and they are organized with certain order by the dependency.



## Implementation

Here we list the 11 module and its functionality. 

| Name          | Functionality                                                | Module |
| ------------- | ------------------------------------------------------------ | ------ |
| chrome_rom    | Deal with ++chrome_rom. If this flag on, set reset vector to certain encoded value. | HTIF_T |
| chip_config   | Deal with ++chip_config. Follow with a string. Parse the string and extract relevant info to init processors and dtb. | HTIF_T |
| load_flash    | Deal with ++load_flash. Follow with a integer and set  the start_pc to that integer | HTIF_T |
| mems_config   | Deal with ++mems. Parse the memory configuration string and initialize the mem_t, then add the mem_t to bus. Note that "-m" is ignored if ++mems exists. If use -m flag only, Demand (1). will fail, which is not recommended. | HTIF_T |
| load_file     | Deal with ++load_files. Follow with a string with format: file1\_\_addr1,file2\_\_addr2,...,fileN\_\_addrN. It will read each file and write the whole binary to each corresponding addr. | HTIF_T |
| load_program  | Deal with ++filename, which is an elf file. htif_t::load_program() is call to read the file. The shift_file value will be added to the original addr in elf if that elf is bbl0. | HTIF_T |
| proc_init     | This is a internal function. Which take information from chip_config and chrome_rom to initialize the processors. But due to the derive relationship of classes. proc_init is limit to put at sim_t. So, all module depending to proc_init must be put at sim_t. | SIM_T  |
| make_dtb      | Take the chips/mems/procs info to generate dts, and use dts to build dtb. | SIM_T  |
| Clint_init    | Take dtb string to initialize the interrupt services.        | SIM_T  |
| pmp_gran init | Further initialize the procs vector by dtb string.           | SIM_T  |
| set_rom       | Use the customized dtb / start_pc /  rstvec to build customized rom and write to the bus. | SIM_T  |

#### Module Input Output 

| Name         | In                       | Out                           |
| ------------ | ------------------------ | ----------------------------- |
| chrome_rom   | **USER ++chrome**        | rstvec                        |
| chip_config  | **USER ++chip_config=**  | proc_num,frequency,isa_string |
| mems_config  | **USER ++mems=**         | **mems,bus.add(mems)**        |
| load_flash   | **USER ++load_flash**    | start_pc                      |
| load_program | **USER ++filename**      | **memif->write(elf)**         |
| load_file    | **USER ++load_files=**   | **memif->write(files)**       |
| proc_init    | rstvec,isa,proc_num,mems | **procs**                     |
| make_dtb     | procs,frequency,         | dtb // string                 |
| clint_init   | dtb,procs,frequency      | **bus.add(clint)**            |
| pmp_ran_init | dtb,procs                | **procs.set_pmp()**           |
| set_rom      | start_pc,rstvec,dtb      | **bus.add(boot_rom)**         |

Bolden texts are output that influence the latter simulator execution or input need outside influence. Normal texts are internal information.

![image-20200913234720795](C:\Users\yuchengchen\AppData\Roaming\Typora\typora-user-images\image-20200913234720795.png)

This graph, red arrow mean dependency. Green arrow means external output. 





## Appendix: Running Log

Open the #define VF_DEBUG, the VFSpike will output the detail of all the initialization process. Below is the detailed info.

```shell
./spike --pc=0x93000000 -m0x90000000:0x10000000,0x80000000:0x20000000 ++/home/yuchengchen/vf/device/sw/bbl/build/bbl0 ++load_rom ++chrome_rom ++load_files=/home/yuchengchen/vf/keys/root_key.vbpubk__0x95600000,/home/yuchengchen/vf/keys/firmware.vblock__0x95601000,/home/yuchengchen/vf/device/sw/bbl/build/bbl1__0x95603000,/home/yuchengchen/vf/keys/firmware_v2.vblock__0x95701000,/home/yuchengchen/vf/device/sw/bbl/build/bbl1__0x95703000,/home/yuchengchen/vf/device/sw/lib/gptk.bin__0x90000000  ++chip_config=nc_1__f_1000__xlen_64__isa_rv64imafdc  ++shift_file ++mems=0x90000000:0x10000000,0x80000000:0x20000000 ++load_to_flash

############chrome_rom start############
    rstvec <- 0xa, committed
############chrome_rom end############

############chip config start############
    argmap[chip_config]=nc_1__f_1000__xlen_64__isa_rv64imafdc
        underline_separte src=nc_1__f_1000__xlen_64__isa_rv64imafdc
        underline_separte result:"nc_1","f_1000__xlen_64__isa_rv64imafdc"
        ret1=nc_1,ret2=f_1000__xlen_64__isa_rv64imafdc
        flag=0,i=6
        tmpsub=nc_1
  pair_first=nc,pair_second=1
    cpu_num <- 1
        underline_separte src=f_1000__xlen_64__isa_rv64imafdc
        underline_separte result:"f_1000","xlen_64__isa_rv64imafdc"
        ret1=f_1000,ret2=xlen_64__isa_rv64imafdc
        flag=0,i=14
        tmpsub=f_1000
  pair_first=f,pair_second=1000
    freq <- 1000
        underline_separte src=xlen_64__isa_rv64imafdc
        underline_separte result:"xlen_64","isa_rv64imafdc"
        ret1=xlen_64,ret2=isa_rv64imafdc
        flag=0,i=23
        tmpsub=xlen_64
  pair_first=xlen,pair_second=64
        underline_separte src=isa_rv64imafdc
underline_separate no find __ indicator
        ret1=isa_rv64imafdc,ret2=
        flag=1,i=39
        tmpsub=isa_rv64imafdc
  pair_first=isa,pair_second=rv64imafdc
    htif_isa <- rv64imafdc
commit result:
    cpu_num=1,freq=1000,htif_isa=rv64imafdc
chip_config end---
############mems_config start############
helper_make_mems
        mb=90000000,p=:0x10000000,0x80000000:0x20000000
        res.push_back(90000000,(mem_t(268435456)))
initialuze a mems:(size:0x10000000)
        res.push_back(80000000,(mem_t(536870912)))
initialuze a mems:(size:0x20000000)
detecter mems final first:0x80000000,memsize=0x20000000
~~~~add_device start~~~~
    addr=0x80000000,dev=0x7fffeb15a290
~~~~add_device end~~~
        htifmems :commit bus.add_device 80000000,(20000000,)
############mems_config end############

############make_flash_addr start############
    argmap[loadflash=]=    start_pc <- 7f7c1c610f00, committed
make_flash_addr end############

~~~~add_device start~~~~
    addr=0x0,dev=0x7ffff279e1b8
~~~~add_device end~~~
############init_procs start############
    hartids=
    isa=rv64imafdc,priv=MSU,varch=vlen:128,elen:64,slen:128,halted=0
detecter
detecter2
detecter3
############init proc end############

############make_dtb start############
    INSNS_P_R_T=100,freq=1000,initrd_start=0,initrd_end=0
    bootargs=(null)
    procs.pc=2148335616
    htif_mems=reg_t : 2147483648(20000000,)
    dts=/dts-v1/;

/ {
  #address-cells = <2>;
  #size-cells = <2>;
  compatible = "ucbbar,spike-bare-dev";
  model = "ucbbar,spike-bare";
  chosen {
    bootargs = "console=hvc0 earlycon=sbi";
  };
  cpus {
    #address-cells = <1>;
    #size-cells = <0>;
    timebase-frequency = <10>;
    CPU0: cpu@0 {
      device_type = "cpu";
      reg = <0>;
      status = "okay";
      compatible = "riscv";
      riscv,isa = "rv64imafdc";
      mmu-type = "riscv,sv48";
      riscv,pmpregions = <16>;
      riscv,pmpgranularity = <4>;
      clock-frequency = <1000>;
      CPU0_intc: interrupt-controller {
        #interrupt-cells = <1>;
        interrupt-controller;
        compatible = "riscv,cpu-intc";
      };
    };
  };
  memory@80000000 {
    device_type = "memory";
    reg = <0x0 0x80000000 0x0 0x20000000>;
  };
  soc {
    #address-cells = <2>;
    #size-cells = <2>;
    compatible = "ucbbar,spike-bare-soc", "simple-bus";
    ranges;
    clint@2000000 {
      compatible = "riscv,clint0";
      interrupts-extended = <&CPU0_intc 3 &CPU0_intc 7 >;
      reg = <0x0 0x2000000 0x0 0xc0000>;
    };
  };
  htif {
    compatible = "ucb,htif0";
  };
};
,size=1149
,size=1145
############make_dtb end############

############clint_t init start############
~~~~add_device start~~~~
    addr=0x2000000,dev=0x7fffeb1b1560
~~~~add_device end~~~
############clint_t init end############

############pmp init start############
    i:0,set_pmp_num:16,set_pmp_gran:4
############pmp init end############

############load_file start############
    comma_separate input src=/home/yuchengchen/vf/keys/root_key.vbpubk__0x95600000,/home/yuchengchen/vf/keys/firmware.vblock__0x95601000,/home/yuchengchen/vf/device/sw/bbl/build/bbl1__0x95603000,/home/yuchengchen/vf/keys/firmware_v2.vblock__0x95701000,/home/yuchengchen/vf/device/sw/bbl/build/bbl1__0x95703000,/home/yuchengchen/vf/device/sw/lib/gptk.bin__0x90000000
    comma separate output dst=
      /home/yuchengchen/vf/keys/root_key.vbpubk__0x95600000,
      /home/yuchengchen/vf/keys/firmware.vblock__0x95601000,
      /home/yuchengchen/vf/device/sw/bbl/build/bbl1__0x95603000,
      /home/yuchengchen/vf/keys/firmware_v2.vblock__0x95701000,
      /home/yuchengchen/vf/device/sw/bbl/build/bbl1__0x95703000,
      /home/yuchengchen/vf/device/sw/lib/gptk.bin__0x90000000,
        underline_separte src=/home/yuchengchen/vf/keys/root_key.vbpubk__0x95600000
        underline_separte result:"/home/yuchengchen/vf/keys/root_key.vbpubk","0x95600000"
        now openfile fn=/home/yuchengchen/vf/keys/root_key.vbpubk
        openfile end, filesize=2088
    s=/home/yuchengchen/vf/keys/root_key.vbpubk__0x95600000
    mem.write(addr=0x95600000,fsize=2088,bin.get()=0x7f7c1c61c000)
    mem.write committed######
        underline_separte src=/home/yuchengchen/vf/keys/firmware.vblock__0x95601000
        underline_separte result:"/home/yuchengchen/vf/keys/firmware.vblock","0x95601000"
        now openfile fn=/home/yuchengchen/vf/keys/firmware.vblock
        openfile end, filesize=4396
    s=/home/yuchengchen/vf/keys/firmware.vblock__0x95601000
    mem.write(addr=0x95601000,fsize=4396,bin.get()=0x7f7c1c61b000)
    mem.write committed######
        underline_separte src=/home/yuchengchen/vf/device/sw/bbl/build/bbl1__0x95603000
        underline_separte result:"/home/yuchengchen/vf/device/sw/bbl/build/bbl1","0x95603000"
        now openfile fn=/home/yuchengchen/vf/device/sw/bbl/build/bbl1
        openfile end, filesize=508440
    s=/home/yuchengchen/vf/device/sw/bbl/build/bbl1__0x95603000
    mem.write(addr=0x95603000,fsize=508440,bin.get()=0x7f7c1c523000)
    mem.write committed######
        underline_separte src=/home/yuchengchen/vf/keys/firmware_v2.vblock__0x95701000
        underline_separte result:"/home/yuchengchen/vf/keys/firmware_v2.vblock","0x95701000"
        now openfile fn=/home/yuchengchen/vf/keys/firmware_v2.vblock
        openfile end, filesize=4396
    s=/home/yuchengchen/vf/keys/firmware_v2.vblock__0x95701000
    mem.write(addr=0x95701000,fsize=4396,bin.get()=0x7f7c1c61b000)
    mem.write committed######
        underline_separte src=/home/yuchengchen/vf/device/sw/bbl/build/bbl1__0x95703000
        underline_separte result:"/home/yuchengchen/vf/device/sw/bbl/build/bbl1","0x95703000"
        now openfile fn=/home/yuchengchen/vf/device/sw/bbl/build/bbl1
        openfile end, filesize=508440
    s=/home/yuchengchen/vf/device/sw/bbl/build/bbl1__0x95703000
    mem.write(addr=0x95703000,fsize=508440,bin.get()=0x7f7c1c523000)
    mem.write committed######
        underline_separte src=/home/yuchengchen/vf/device/sw/lib/gptk.bin__0x90000000
        underline_separte result:"/home/yuchengchen/vf/device/sw/lib/gptk.bin","0x90000000"
        now openfile fn=/home/yuchengchen/vf/device/sw/lib/gptk.bin
        openfile end, filesize=42619392
    s=/home/yuchengchen/vf/device/sw/lib/gptk.bin__0x90000000
    mem.write(addr=0x90000000,fsize=42619392,bin.get()=0x7f7bb879a000)
    mem.write committed######
############load_file end############


############load program start############
    bbl0 recognizer /home/yuchengchen/vf/device/sw/bbl/build/bbl0
    ############load_elf start#############
        ph[0].p_filesz:0x1733d,paddr:0x80000000,shift:0x0
        zerosize:0x0
        ph[1].p_filesz:0x10dc,paddr:0x80018000,shift:0x0
        zerosize:0xa8524
    ############load_elf end#############
############load program end############

############make_rom start############
add reset_vec to rom:

    -105 2 0 0 -109 -123 2 2 115 37 64 -15 -125 -78 -126 1 103 -128 2 0 0 0 0 0 0 0 0 -109 0 0 0 0 |||
add dtb to rom:
    -105 2 0 0 -109 -123 2 2 115 37 64 -15 -125 -78 -126 1 103 -128 2 0 0 0 0 0 0 0 0 -109 0 0 0 0 -48 13 -2 -19 0 0 4 121 0 0 0 56 0 0 3 -120 0 0 0 40 0 0 0 17 0 0 0 16 0 0 0 0 0 0 0 -15 0 0 3 80 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 3 0 0 0 4 0 0 0 0 0 0 0 2 0 0 0 3 0 0 0 4 0 0 0 15 0 0 0 2 0 0 0 3 0 0 0 22 0 0 0 27 117 99 98 98 97 114 44 115 112 105 107 101 45 98 97 114 101 45 100 101 118 0 0 0 0 0 0 3 0 0 0 18 0 0 0 38 117 99 98 98 97 114 44 115 112 105 107 101 45 98 97 114 101 0 0 0 0 0 0 1 99 104 111 115 101 110 0 0 0 0 0 3 0 0 0 26 0 0 0 44 99 111 110 115 111 108 101 61 104 118 99 48 32 101 97 114 108 121 99 111 110 61 115 98 105 0 0 0 0 0 0 2 0 0 0 1 99 112 117 115 0 0 0 0 0 0 0 3 0 0 0 4 0 0 0 0 0 0 0 1 0 0 0 3 0 0 0 4 0 0 0 15 0 0 0 0 0 0 0 3 0 0 0 4 0 0 0 53 0 0 0 10 0 0 0 1 99 112 117 64 48 0 0 0 0 0 0 3 0 0 0 4 0 0 0 72 99 112 117 0 0 0 0 3 0 0 0 4 0 0 0 84 0 0 0 0 0 0 0 3 0 0 0 5 0 0 0 88 111 107 97 121 0 0 0 0 0 0 0 3 0 0 0 6 0 0 0 27 114 105 115 99 118 0 0 0 0 0 0 3 0 0 0 11 0 0 0 95 114 118 54 52 105 109 97 102 100 99 0 0 0 0 0 3 0 0 0 11 0 0 0 105 114 105 115 99 118 44 115 118 52 56 0 0 0 0 0 3 0 0 0 4 0 0 0 114 0 0 0 16 0 0 0 3 0 0 0 4 0 0 0 -125 0 0 0 4 0 0 0 3 0 0 0 4 0 0 0 -104 0 0 3 -24 0 0 0 1 105 110 116 101 114 114 117 112 116 45 99 111 110 116 114 111 108 108 101 114 0 0 0 0 0 0 0 3 0 0 0 4 0 0 0 -88 0 0 0 1 0 0 0 3 0 0 0 0 0 0 0 -71 0 0 0 3 0 0 0 15 0 0 0 27 114 105 115 99 118 44 99 112 117 45 105 110 116 99 0 0 0 0 0 3 0 0 0 4 0 0 0 -50 0 0 0 1 0 0 0 2 0 0 0 2 0 0 0 2 0 0 0 1 109 101 109 111 114 121 64 56 48 48 48 48 48 48 48 0 0 0 0 3 0 0 0 7 0 0 0 72 109 101 109 111 114 121 0 0 0 0 0 3 0 0 0 16 0 0 0 84 0 0 0 0 -128 0 0 0 0 0 0 0 32 0 0 0 0 0 0 2 0 0 0 1 115 111 99 0 0 0 0 3 0 0 0 4 0 0 0 0 0 0 0 2 0 0 0 3 0 0 0 4 0 0 0 15 0 0 0 2 0 0 0 3 0 0 0 33 0 0 0 27 117 99 98 98 97 114 44 115 112 105 107 101 45 98 97 114 101 45 115 111 99 0 115 105 109 112 108 101 45 98 117 115 0 0 0 0 0 0 0 3 0 0 0 0 0 0 0 -42 0 0 0 1 99 108 105 110 116 64 50 48 48 48 48 48 48 0 0 0 0 0 0 3 0 0 0 13 0 0 0 27 114 105 115 99 118 44 99 108 105 110 116 48 0 0 0 0 0 0 0 3 0 0 0 16 0 0 0 -35 0 0 0 1 0 0 0 3 0 0 0 1 0 0 0 7 0 0 0 3 0 0 0 16 0 0 0 84 0 0 0 0 2 0 0 0 0 0 0 0 0 12 0 0 0 0 0 2 0 0 0 2 0 0 0 1 104 116 105 102 0 0 0 0 0 0 0 3 0 0 0 10 0 0 0 27 117 99 98 44 104 116 105 102 48 0 0 0 0 0 0 2 0 0 0 2 0 0 0 9 35 97 100 100 114 101 115 115 45 99 101 108 108 115 0 35 115 105 122 101 45 99 101 108 108 115 0 99 111 109 112 97 116 105 98 108 101 0 109 111 100 101 108 0 98 111 111 116 97 114 103 115 0 116 105 109 101 98 97 115 101 45 102 114 101 113 117 101 110 99 121 0 100 101 118 105 99 101 95 116 121 112 101 0 114 101 103 0 115 116 97 116 117 115 0 114 105 115 99 118 44 105 115 97 0 109 109 117 45 116 121 112 101 0 114 105 115 99 118 44 112 109 112 114 101 103 105 111 110 115 0 114 105 115 99 118 44 112 109 112 103 114 97 110 117 108 97 114 105 116 121 0 99 108 111 99 107 45 102 114 101 113 117 101 110 99 121 0 35 105 110 116 101 114 114 117 112 116 45 99 101 108 108 115 0 105 110 116 101 114 114 117 112 116 45 99 111 110 116 114 111 108 108 101 114 0 112 104 97 110 100 108 101 0 114 97 110 103 101 115 0 105 110 116 101 114 114 117 112 116 115 45 101 120 116 101 110 100 101 100 0 |||
add align to rom:
    -105 2 0 0 -109 -123 2 2 115 37 64 -15 -125 -78 -126 1 103 -128 2 0 0 0 0 0 0 0 0 -109 0 0 0 0 -48 13 -2 -19 0 0 4 121 0 0 0 56 0 0 3 -120 0 0 0 40 0 0 0 17 0 0 0 16 0 0 0 0 0 0 0 -15 0 0 3 80 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 3 0 0 0 4 0 0 0 0 0 0 0 2 0 0 0 3 0 0 0 4 0 0 0 15 0 0 0 2 0 0 0 3 0 0 0 22 0 0 0 27 117 99 98 98 97 114 44 115 112 105 107 101 45 98 97 114 101 45 100 101 118 0 0 0 0 0 0 3 0 0 0 18 0 0 0 38 117 99 98 98 97 114 44 115 112 105 107 101 45 98 97 114 101 0 0 0 0 0 0 1 99 104 111 115 101 110 0 0 0 0 0 3 0 0 0 26 0 0 0 44 99 111 110 115 111 108 101 61 104 118 99 48 32 101 97 114 108 121 99 111 110 61 115 98 105 0 0 0 0 0 0 2 0 0 0 1 99 112 117 115 0 0 0 0 0 0 0 3 0 0 0 4 0 0 0 0 0 0 0 1 0 0 0 3 0 0 0 4 0 0 0 15 0 0 0 0 0 0 0 3 0 0 0 4 0 0 0 53 0 0 0 10 0 0 0 1 99 112 117 64 48 0 0 0 0 0 0 3 0 0 0 4 0 0 0 72 99 112 117 0 0 0 0 3 0 0 0 4 0 0 0 84 0 0 0 0 0 0 0 3 0 0 0 5 0 0 0 88 111 107 97 121 0 0 0 0 0 0 0 3 0 0 0 6 0 0 0 27 114 105 115 99 118 0 0 0 0 0 0 3 0 0 0 11 0 0 0 95 114 118 54 52 105 109 97 102 100 99 0 0 0 0 0 3 0 0 0 11 0 0 0 105 114 105 115 99 118 44 115 118 52 56 0 0 0 0 0 3 0 0 0 4 0 0 0 114 0 0 0 16 0 0 0 3 0 0 0 4 0 0 0 -125 0 0 0 4 0 0 0 3 0 0 0 4 0 0 0 -104 0 0 3 -24 0 0 0 1 105 110 116 101 114 114 117 112 116 45 99 111 110 116 114 111 108 108 101 114 0 0 0 0 0 0 0 3 0 0 0 4 0 0 0 -88 0 0 0 1 0 0 0 3 0 0 0 0 0 0 0 -71 0 0 0 3 0 0 0 15 0 0 0 27 114 105 115 99 118 44 99 112 117 45 105 110 116 99 0 0 0 0 0 3 0 0 0 4 0 0 0 -50 0 0 0 1 0 0 0 2 0 0 0 2 0 0 0 2 0 0 0 1 109 101 109 111 114 121 64 56 48 48 48 48 48 48 48 0 0 0 0 3 0 0 0 7 0 0 0 72 109 101 109 111 114 121 0 0 0 0 0 3 0 0 0 16 0 0 0 84 0 0 0 0 -128 0 0 0 0 0 0 0 32 0 0 0 0 0 0 2 0 0 0 1 115 111 99 0 0 0 0 3 0 0 0 4 0 0 0 0 0 0 0 2 0 0 0 3 0 0 0 4 0 0 0 15 0 0 0 2 0 0 0 3 0 0 0 33 0 0 0 27 117 99 98 98 97 114 44 115 112 105 107 101 45 98 97 114 101 45 115 111 99 0 115 105 109 112 108 101 45 98 117 115 0 0 0 0 0 0 0 3 0 0 0 0 0 0 0 -42 0 0 0 1 99 108 105 110 116 64 50 48 48 48 48 48 48 0 0 0 0 0 0 3 0 0 0 13 0 0 0 27 114 105 115 99 118 44 99 108 105 110 116 48 0 0 0 0 0 0 0 3 0 0 0 16 0 0 0 -35 0 0 0 1 0 0 0 3 0 0 0 1 0 0 0 7 0 0 0 3 0 0 0 16 0 0 0 84 0 0 0 0 2 0 0 0 0 0 0 0 0 12 0 0 0 0 0 2 0 0 0 2 0 0 0 1 104 116 105 102 0 0 0 0 0 0 0 3 0 0 0 10 0 0 0 27 117 99 98 44 104 116 105 102 48 0 0 0 0 0 0 2 0 0 0 2 0 0 0 9 35 97 100 100 114 101 115 115 45 99 101 108 108 115 0 35 115 105 122 101 45 99 101 108 108 115 0 99 111 109 112 97 116 105 98 108 101 0 109 111 100 101 108 0 98 111 111 116 97 114 103 115 0 116 105 109 101 98 97 115 101 45 102 114 101 113 117 101 110 99 121 0 100 101 118 105 99 101 95 116 121 112 101 0 114 101 103 0 115 116 97 116 117 115 0 114 105 115 99 118 44 105 115 97 0 109 109 117 45 116 121 112 101 0 114 105 115 99 118 44 112 109 112 114 101 103 105 111 110 115 0 114 105 115 99 118 44 112 109 112 103 114 97 110 117 108 97 114 105 116 121 0 99 108 111 99 107 45 102 114 101 113 117 101 110 99 121 0 35 105 110 116 101 114 114 117 112 116 45 99 101 108 108 115 0 105 110 116 101 114 114 117 112 116 45 99 111 110 116 114 111 108 108 101 114 0 112 104 97 110 100 108 101 0 114 97 110 103 101 115 0 105 110 116 101 114 114 117 112 116 115 45 101 120 116 101 110 100 101 100 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 |||
~~~~add_device start~~~~
    addr=0x800d0000,dev=0x7fffeb1c7ff0
~~~~add_device end~~~
    commit result
    rstvec=0x800d0000,boot_rom.get()=0x7fffeb1c7ff0,rom.size()=0x1000
############make_rom end ############
```

