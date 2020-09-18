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

![1.png](./figs/1.png)

This graph, red arrow mean dependency. Green arrow means external output. 





## Appendix: Running Log
brun.sh: 1: brun.sh: ./build/make: not found
SPIKEMAIN_make_mems
    mb=800,p=
initialuze a mems:(size:0x80000000)
############chrome_rom start############
    rstvec <- 0x1000, committed
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
    addr=0x80000000,dev=0x7ffff55a32b0
~~~~add_device end~~~
        htifmems :commit bus.add_device 80000000,(20000000,)
############mems_config end############

############make_flash_addr start############
    start_pc <- 93000000, committed
#############make_flash_addr end############

~~~~add_device start~~~~
    addr=0x0,dev=0x7ffffdae6b38
~~~~add_device end~~~
############init_procs start############
    hartids=
    isa=rv64imafdc,priv=MSU,varch=vlen:128,elen:64,slen:128,halted=0
############init proc end############

############make_dtb start############
    INSNS_P_R_T=100,freq=1000,initrd_start=0,initrd_end=0
    bootargs=(null)
    procs.pc=4096
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
    addr=0x2000000,dev=0x7ffff55fa4c0
~~~~add_device end~~~
############clint_t init end############

############pmp init start############
    i:0,set_pmp_num:16,set_pmp_gran:4
############pmp init end############

############load_file start############
    comma_separate input src=/home/yuchengchen/vfbackup/vf/keys/root_key.vbpubk__0x95600000,/home/yuchengchen/vfbackup/vf/keys/firmware.vblock__0x95601000,/home/yuchengchen/vfbackup/vf/keys/bbl1__0x95603000,/home/yuchengchen/vfbackup/vf/keys/firmware_v2.vblock__0x95701000,/home/yuchengchen/vfbackup/vf/keys/bbl1__0x95703000,/home/yuchengchen/vfbackup/vf/device/sw/lib/gptk.bin__0x90000000
    comma separate output dst=
      /home/yuchengchen/vfbackup/vf/keys/root_key.vbpubk__0x95600000,
      /home/yuchengchen/vfbackup/vf/keys/firmware.vblock__0x95601000,
      /home/yuchengchen/vfbackup/vf/keys/bbl1__0x95603000,
      /home/yuchengchen/vfbackup/vf/keys/firmware_v2.vblock__0x95701000,
      /home/yuchengchen/vfbackup/vf/keys/bbl1__0x95703000,
      /home/yuchengchen/vfbackup/vf/device/sw/lib/gptk.bin__0x90000000,
        underline_separte src=/home/yuchengchen/vfbackup/vf/keys/root_key.vbpubk__0x95600000
        underline_separte result:"/home/yuchengchen/vfbackup/vf/keys/root_key.vbpubk","0x95600000"
        now openfile fn=/home/yuchengchen/vfbackup/vf/keys/root_key.vbpubk
        openfile end, filesize=2088
    s=/home/yuchengchen/vfbackup/vf/keys/root_key.vbpubk__0x95600000
    mem.write(addr=0x95600000,fsize=2088,bin.get()=0x7f38166cd000)
    mem.write committed######
        underline_separte src=/home/yuchengchen/vfbackup/vf/keys/firmware.vblock__0x95601000
        underline_separte result:"/home/yuchengchen/vfbackup/vf/keys/firmware.vblock","0x95601000"
        now openfile fn=/home/yuchengchen/vfbackup/vf/keys/firmware.vblock
        openfile end, filesize=4396
    s=/home/yuchengchen/vfbackup/vf/keys/firmware.vblock__0x95601000
    mem.write(addr=0x95601000,fsize=4396,bin.get()=0x7f38166cc000)
    mem.write committed######
        underline_separte src=/home/yuchengchen/vfbackup/vf/keys/bbl1__0x95603000
        underline_separte result:"/home/yuchengchen/vfbackup/vf/keys/bbl1","0x95603000"
        now openfile fn=/home/yuchengchen/vfbackup/vf/keys/bbl1
        openfile end, filesize=516168
    s=/home/yuchengchen/vfbackup/vf/keys/bbl1__0x95603000
    mem.write(addr=0x95603000,fsize=516168,bin.get()=0x7f38165aa000)
    mem.write committed######
        underline_separte src=/home/yuchengchen/vfbackup/vf/keys/firmware_v2.vblock__0x95701000
        underline_separte result:"/home/yuchengchen/vfbackup/vf/keys/firmware_v2.vblock","0x95701000"
        now openfile fn=/home/yuchengchen/vfbackup/vf/keys/firmware_v2.vblock
        openfile end, filesize=4396
    s=/home/yuchengchen/vfbackup/vf/keys/firmware_v2.vblock__0x95701000
    mem.write(addr=0x95701000,fsize=4396,bin.get()=0x7f38166cc000)
    mem.write committed######
        underline_separte src=/home/yuchengchen/vfbackup/vf/keys/bbl1__0x95703000
        underline_separte result:"/home/yuchengchen/vfbackup/vf/keys/bbl1","0x95703000"
        now openfile fn=/home/yuchengchen/vfbackup/vf/keys/bbl1
        openfile end, filesize=516168
    s=/home/yuchengchen/vfbackup/vf/keys/bbl1__0x95703000
    mem.write(addr=0x95703000,fsize=516168,bin.get()=0x7f38165aa000)
    mem.write committed######
        underline_separte src=/home/yuchengchen/vfbackup/vf/device/sw/lib/gptk.bin__0x90000000
        underline_separte result:"/home/yuchengchen/vfbackup/vf/device/sw/lib/gptk.bin","0x90000000"
        now openfile fn=/home/yuchengchen/vfbackup/vf/device/sw/lib/gptk.bin
        openfile end, filesize=42619392
    s=/home/yuchengchen/vfbackup/vf/device/sw/lib/gptk.bin__0x90000000
    mem.write(addr=0x90000000,fsize=42619392,bin.get()=0x7f37629aa000)
    mem.write committed######
############load_file end############


############load program start############
    bbl0 recognizer /home/yuchengchen/vfbackup/vf/keys/bbl0
    loading bbl0, shift_value=0x13000000
    ############load_elf start#############
        ph[0].p_filesz:0x172b8,paddr:0x80000000,shift:0x13000000,final_addr:0x93000000
        zerosize:0x0
        ph[1].p_filesz:0x10c4,paddr:0x80018000,shift:0x13000000,final_addr:0x93018000
        zerosize:0xa853c
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
    addr=0x1000,dev=0x7ffff56116c0
~~~~add_device end~~~
    commit result
    rstvec=0x1000,start_pc=0x93000000,boot_rom.get()=0x7ffff56116c0,rom.size()=0x1000
############make_rom end ############

: rs
bbl loader
Setup memory protection
init
stage 1
stage 2
stage 3
stage 4
Verifying Firmware A
stage 5
Loading Firmware A
Jumping to Loaded Firmware
stage 6
kernel size to read 15790080 kernel destination 80fff000
Done vboot
[    0.000000] OF: fdt: Ignoring memory range 0x80000000 - 0x81000000
[    0.000000] Linux version 5.2.0 (root@ea9b93c5e564) (gcc version 8.3.0 (GCC)) #31 SMP Sun Nov 10 15:21:59 PST 2019
[    0.000000] earlycon: sbi0 at I/O port 0x0 (options '')
[    0.000000] printk: bootconsole [sbi0] enabled
[    0.000000] initrd not found or empty - disabling initrd
[    0.000000] Zone ranges:
[    0.000000]   DMA32    [mem 0x0000000081000000-0x000000008fffffff]
[    0.000000]   Normal   empty
[    0.000000] Movable zone start for each node
[    0.000000] Early memory node ranges
[    0.000000]   node   0: [mem 0x0000000081000000-0x000000008fffffff]
[    0.000000] Initmem setup node 0 [mem 0x0000000081000000-0x000000008fffffff]
[    0.000000] software IO TLB: mapped [mem 0x8bcb6000-0x8fcb6000] (64MB)
[    0.000000] elf_hwcap is 0x112d
[    0.000000] percpu: Embedded 16 pages/cpu s27608 r8192 d29736 u65536
[    0.000000] Built 1 zonelists, mobility grouping on.  Total pages: 60600
[    0.000000] Kernel command line: console=hvc0 earlycon=sbi
[    0.000000] Dentry cache hash table entries: 32768 (order: 6, 262144 bytes)
[    0.000000] Inode-cache hash table entries: 16384 (order: 5, 131072 bytes)
[    0.000000] Sorting __ex_table...
[    0.000000] Memory: 161856K/245760K available (3686K kernel code, 173K rwdata, 891K rodata, 8666K init, 730K bss, 83904K reserved, 0K cma-reserved)
[    0.000000] SLUB: HWalign=64, Order=0-3, MinObjects=0, CPUs=1, Nodes=1
[    0.000000] rcu: Hierarchical RCU implementation.
[    0.000000] rcu:     RCU restricting CPUs from NR_CPUS=8 to nr_cpu_ids=1.
[    0.000000] rcu: RCU calculated value of scheduler-enlistment delay is 25 jiffies.
[    0.000000] rcu: Adjusting geometry for rcu_fanout_leaf=16, nr_cpu_ids=1
[    0.000000] NR_IRQS: 0, nr_irqs: 0, preallocated irqs: 0
[    0.000000] riscv_timer_init_dt: Registering clocksource cpuid [0] hartid [0]
[    0.000000] clocksource: riscv_clocksource: mask: 0xffffffffffffffff max_cycles: 0x1358c14ce, max_idle_ns: 231103634895000000 ns
[    0.000000] sched_clock: 64 bits at 10 Hz, resolution 100000000ns, wraps every 288230376150000000ns
[  525.000000] printk: console [hvc0] enabled
[  525.000000] printk: console [hvc0] enabled
[ 1000.000000] printk: bootconsole [sbi0] disabled
[ 1000.000000] printk: bootconsole [sbi0] disabled
[ 1720.000000] calibrate_delay_direct() failed to get a good estimate for loops_per_jiffy.
[ 1720.000000] Probably due to long platform interrupts. Consider using "lpj=" boot option.
[ 2640.000000] Calibrating delay loop... 0.02 BogoMIPS (lpj=50)
[ 4360.000000] pid_max: default: 32768 minimum: 301
[ 4715.000000] Mount-cache hash table entries: 512 (order: 0, 4096 bytes)
[ 5095.000000] Mountpoint-cache hash table entries: 512 (order: 0, 4096 bytes)
[ 6200.000000] rcu: Hierarchical SRCU implementation.
[ 6595.000000] smp: Bringing up secondary CPUs ...
[ 6860.000000] smp: Brought up 1 node, 1 CPU
[ 7280.000000] devtmpfs: initialized
[ 7795.000000] random: get_random_u32 called from bucket_table_alloc.isra.10+0x74/0x1fc with crng_init=0
[ 8400.000000] clocksource: jiffies: mask: 0xffffffff max_cycles: 0xffffffff, max_idle_ns: 7645041785100000 ns
[ 8965.000000] futex hash table entries: 256 (order: 2, 16384 bytes)
[13480.000000] SCSI subsystem initialized
[26210.000000] rcu: INFO: rcu_sched detected stalls on CPUs/tasks:
[26545.000000]  (detected by 0, t=5252 jiffies, g=-1055, q=1)
[26865.000000] rcu: All QSes seen, last rcu_sched kthread activity 5252 (4294898262-4294893010), jiffies_till_next_fqs=1, root ->qsmask 0x0
[27565.000000] migration/0     R  running task        0    11      2 0x00000000
[27970.000000] Call Trace:
[28115.000000] [<ffffffe0008795b4>] walk_stackframe+0x0/0xfc
[28420.000000] [<ffffffe0008797e8>] show_stack+0x3c/0x50
[28715.000000] [<ffffffe0008a4b38>] sched_show_task+0xe8/0x174
[29030.000000] [<ffffffe0008e02c0>] rcu_sched_clock_irq+0x66c/0x6bc
[29375.000000] [<ffffffe0008e6d04>] update_process_times+0x38/0x78
[29715.000000] [<ffffffe0008f389c>] tick_periodic+0x60/0x128
[30025.000000] [<ffffffe0008f39f4>] tick_handle_periodic+0x90/0x94
[30365.000000] [<ffffffe000bd9b10>] riscv_timer_interrupt+0x44/0x54
[30710.000000] [<ffffffe000c0f7f8>] do_IRQ+0xf0/0x100
[30985.000000] [<ffffffe0008781fc>] ret_from_exception+0x0/0x10
[31310.000000] [<ffffffe0008bf600>] __wake_up_locked+0x20/0x30
[31635.000000] rcu: rcu_sched kthread starved for 5252 jiffies! g-1055 f0x2 RCU_GP_INIT(4) ->state=0x0 ->cpu=0
[32190.000000] rcu: RCU grace-period kthread stack dump:
[32480.000000] rcu_sched       R  running task        0    10      2 0x00000000
[32885.000000] Call Trace:
[33025.000000] [<ffffffe000c0a7ec>] __schedule+0x1a4/0x3ac
[33325.000000] [<ffffffe000c0ae04>] preempt_schedule_common+0x18/0x34
[33680.000000] [<ffffffe000c0ae70>] _cond_resched+0x50/0x64
[33985.000000] [<ffffffe0008dedfc>] rcu_gp_kthread+0x35c/0xbe4
[34305.000000] [<ffffffe00089e3f0>] kthread+0x114/0x128
[34590.000000] [<ffffffe0008781fc>] ret_from_exception+0x0/0x10
[73140.000000] rcu: INFO: rcu_sched detected stalls on CPUs/tasks:
[73475.000000]  (detected by 0, t=21007 jiffies, g=-1055, q=1)
[73800.000000] rcu: All QSes seen, last rcu_sched kthread activity 21007 (4294914017-4294893010), jiffies_till_next_fqs=1, root ->qsmask 0x0
[74505.000000] migration/0     R  running task        0    11      2 0x00000000
[74910.000000] Call Trace:
[75055.000000] [<ffffffe0008795b4>] walk_stackframe+0x0/0xfc
[75360.000000] [<ffffffe0008797e8>] show_stack+0x3c/0x50
[75655.000000] [<ffffffe0008a4b38>] sched_show_task+0xe8/0x174
[75970.000000] [<ffffffe0008e02c0>] rcu_sched_clock_irq+0x66c/0x6bc
[76315.000000] [<ffffffe0008e6d04>] update_process_times+0x38/0x78
[76655.000000] [<ffffffe0008f389c>] tick_periodic+0x60/0x128
[76965.000000] [<ffffffe0008f39f4>] tick_handle_periodic+0x90/0x94
[77305.000000] [<ffffffe000bd9b10>] riscv_timer_interrupt+0x44/0x54
[77650.000000] [<ffffffe000c0f7f8>] do_IRQ+0xf0/0x100
[77925.000000] [<ffffffe0008781fc>] ret_from_exception+0x0/0x10
[78250.000000] [<ffffffe0008bf600>] __wake_up_locked+0x20/0x30
[78575.000000] rcu: rcu_sched kthread starved for 21007 jiffies! g-1055 f0x2 RCU_GP_INIT(4) ->state=0x0 ->cpu=0
[79135.000000] rcu: RCU grace-period kthread stack dump:
[79425.000000] rcu_sched       R  running task        0    10      2 0x00000000
[79830.000000] Call Trace:
[79970.000000] [<ffffffe000c0a7ec>] __schedule+0x1a4/0x3ac
[80270.000000] [<ffffffe000c0ae04>] preempt_schedule_common+0x18/0x34
[80625.000000] [<ffffffe000c0ae70>] _cond_resched+0x50/0x64
[80930.000000] [<ffffffe0008dedfc>] rcu_gp_kthread+0x35c/0xbe4
[81250.000000] [<ffffffe00089e3f0>] kthread+0x114/0x128
[81535.000000] [<ffffffe0008781fc>] ret_from_exception+0x0/0x10
[120085.000000] rcu: INFO: rcu_sched detected stalls on CPUs/tasks:
[120430.000000]         (detected by 0, t=36762 jiffies, g=-1055, q=1)
[120760.000000] rcu: All QSes seen, last rcu_sched kthread activity 36762 (4294929772-4294893010), jiffies_till_next_fqs=1, root ->qsmask 0x0
[121470.000000] migration/0     R  running task        0    11      2 0x00000000
[121880.000000] Call Trace:
[122030.000000] [<ffffffe0008795b4>] walk_stackframe+0x0/0xfc
[122340.000000] [<ffffffe0008797e8>] show_stack+0x3c/0x50
[122640.000000] [<ffffffe0008a4b38>] sched_show_task+0xe8/0x174
[122960.000000] [<ffffffe0008e02c0>] rcu_sched_clock_irq+0x66c/0x6bc
[123310.000000] [<ffffffe0008e6d04>] update_process_times+0x38/0x78
[123655.000000] [<ffffffe0008f389c>] tick_periodic+0x60/0x128
[123970.000000] [<ffffffe0008f39f4>] tick_handle_periodic+0x90/0x94
[124315.000000] [<ffffffe000bd9b10>] riscv_timer_interrupt+0x44/0x54
[124665.000000] [<ffffffe000c0f7f8>] do_IRQ+0xf0/0x100
[124945.000000] [<ffffffe0008781fc>] ret_from_exception+0x0/0x10
[125275.000000] [<ffffffe0008bf600>] __wake_up_locked+0x20/0x30
[125605.000000] rcu: rcu_sched kthread starved for 36762 jiffies! g-1055 f0x2 RCU_GP_INIT(4) ->state=0x0 ->cpu=0
[126170.000000] rcu: RCU grace-period kthread stack dump:
[126465.000000] rcu_sched       R  running task        0    10      2 0x00000000
[126875.000000] Call Trace:
[127020.000000] [<ffffffe000c0a7ec>] __schedule+0x1a4/0x3ac
[127325.000000] [<ffffffe000c0ae04>] preempt_schedule_common+0x18/0x34
[127685.000000] [<ffffffe000c0ae70>] _cond_resched+0x50/0x64
[127995.000000] [<ffffffe0008dedfc>] rcu_gp_kthread+0x35c/0xbe4
[128320.000000] [<ffffffe00089e3f0>] kthread+0x114/0x128
[128610.000000] [<ffffffe0008781fc>] ret_from_exception+0x0/0x10
[167175.000000] rcu: INFO: rcu_sched detected stalls on CPUs/tasks:
[167520.000000]         (detected by 0, t=52517 jiffies, g=-1055, q=1)
[167850.000000] rcu: All QSes seen, last rcu_sched kthread activity 52517 (4294945527-4294893010), jiffies_till_next_fqs=1, root ->qsmask 0x0
[168560.000000] migration/0     R  running task        0    11      2 0x00000000
[168970.000000] Call Trace:
[169120.000000] [<ffffffe0008795b4>] walk_stackframe+0x0/0xfc
[169430.000000] [<ffffffe0008797e8>] show_stack+0x3c/0x50
[169730.000000] [<ffffffe0008a4b38>] sched_show_task+0xe8/0x174
[170050.000000] [<ffffffe0008e02c0>] rcu_sched_clock_irq+0x66c/0x6bc
[170400.000000] [<ffffffe0008e6d04>] update_process_times+0x38/0x78
[170745.000000] [<ffffffe0008f389c>] tick_periodic+0x60/0x128
[171060.000000] [<ffffffe0008f39f4>] tick_handle_periodic+0x90/0x94
[171405.000000] [<ffffffe000bd9b10>] riscv_timer_interrupt+0x44/0x54
[171755.000000] [<ffffffe000c0f7f8>] do_IRQ+0xf0/0x100
[172035.000000] [<ffffffe0008781fc>] ret_from_exception+0x0/0x10
[172365.000000] [<ffffffe0008bf600>] __wake_up_locked+0x20/0x30
[172695.000000] rcu: rcu_sched kthread starved for 52517 jiffies! g-1055 f0x2 RCU_GP_INIT(4) ->state=0x0 ->cpu=0
[173260.000000] rcu: RCU grace-period kthread stack dump:
[173555.000000] rcu_sched       R  running task        0    10      2 0x00000000
[173965.000000] Call Trace:
[174110.000000] [<ffffffe000c0a7ec>] __schedule+0x1a4/0x3ac
[174415.000000] [<ffffffe000c0ae04>] preempt_schedule_common+0x18/0x34
[174775.000000] [<ffffffe000c0ae70>] _cond_resched+0x50/0x64
[175085.000000] [<ffffffe0008dedfc>] rcu_gp_kthread+0x35c/0xbe4
[175410.000000] [<ffffffe00089e3f0>] kthread+0x114/0x128
[175700.000000] [<ffffffe0008781fc>] ret_from_exception+0x0/0x10
[214265.000000] rcu: INFO: rcu_sched detected stalls on CPUs/tasks:
[214610.000000]         (detected by 0, t=68272 jiffies, g=-1055, q=1)
[214940.000000] rcu: All QSes seen, last rcu_sched kthread activity 68272 (4294961282-4294893010), jiffies_till_next_fqs=1, root ->qsmask 0x0
[215650.000000] migration/0     R  running task        0    11      2 0x00000000
[216060.000000] Call Trace:
[216210.000000] [<ffffffe0008795b4>] walk_stackframe+0x0/0xfc
[216520.000000] [<ffffffe0008797e8>] show_stack+0x3c/0x50
[216820.000000] [<ffffffe0008a4b38>] sched_show_task+0xe8/0x174
[217140.000000] [<ffffffe0008e02c0>] rcu_sched_clock_irq+0x66c/0x6bc
[217490.000000] [<ffffffe0008e6d04>] update_process_times+0x38/0x78
[217835.000000] [<ffffffe0008f389c>] tick_periodic+0x60/0x128
[218150.000000] [<ffffffe0008f39f4>] tick_handle_periodic+0x90/0x94
[218495.000000] [<ffffffe000bd9b10>] riscv_timer_interrupt+0x44/0x54
[218845.000000] [<ffffffe000c0f7f8>] do_IRQ+0xf0/0x100
[219125.000000] [<ffffffe0008781fc>] ret_from_exception+0x0/0x10
[219455.000000] [<ffffffe0008bf600>] __wake_up_locked+0x20/0x30
[219785.000000] rcu: rcu_sched kthread starved for 68272 jiffies! g-1055 f0x2 RCU_GP_INIT(4) ->state=0x0 ->cpu=0
[220350.000000] rcu: RCU grace-period kthread stack dump:
[220645.000000] rcu_sched       R  running task        0    10      2 0x00000000
[221055.000000] Call Trace:
[221200.000000] [<ffffffe000c0a7ec>] __schedule+0x1a4/0x3ac
[221505.000000] [<ffffffe000c0ae04>] preempt_schedule_common+0x18/0x34
[221865.000000] [<ffffffe000c0ae70>] _cond_resched+0x50/0x64
[222175.000000] [<ffffffe0008dedfc>] rcu_gp_kthread+0x35c/0xbe4
[222500.000000] [<ffffffe00089e3f0>] kthread+0x114/0x128
[222790.000000] [<ffffffe0008781fc>] ret_from_exception+0x0/0x10
```
Successfully run into kernel.

