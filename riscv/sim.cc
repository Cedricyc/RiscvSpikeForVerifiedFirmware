// See LICENSE for license details.

#include "sim.h"
#include "mmu.h"
#include "dts.h"
#include "remote_bitbang.h"
#include "byteorder.h"
#include <fstream>
#include <map>
#include <iostream>
#include <sstream>
#include <climits>
#include <cstdlib>
#include <cassert>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

volatile bool ctrlc_pressed = false;
static void handle_signal(int sig)
{
  if (ctrlc_pressed)
    exit(-1);
  ctrlc_pressed = true;
  signal(sig, &handle_signal);
}

void sim_t::procs_init(std::vector<int> const &hartids,const char* isa,const char* priv,const char* varch,bool halted) 
{
  #ifdef VF_DEBUG 
  printf("############init_procs start############\n    hartids=");
  PRINT_STD_VECTOR(hartids);
  printf("\n    isa=%s,priv=%s,varch=%s,halted=%d\n",isa,priv,varch,halted);
  #endif
  if (! (hartids.empty() || hartids.size() == procs.size())) {
      std::cerr << "Number of specified hartids ("
                << hartids.size()
                << ") doesn't match number of processors ("
                << procs.size() << ").\n";
      exit(1);
  }
  // (modified 6)     !!!! this need in fesvr!!! 

  for (size_t i = 0; i < procs.size(); i++) {
    int hart_id = hartids.empty() ? i : hartids[i];
    procs[i] = new processor_t(isa, priv, varch, 
                              this, hart_id, halted,
                              log_file.get(),rstvec);
  }        
  #ifdef VF_DEBUG
  puts("############init proc end############\n");
  #endif
}

sim_t::sim_t(const char* isa, const char* priv, const char* varch,
             /*size_t nprocs,*/ bool halted, bool real_time_clint,
             reg_t initrd_start, reg_t initrd_end, const char* bootargs,
             reg_t start_pc, std::vector<std::pair<reg_t, mem_t*>> mems,
             std::vector<std::pair<reg_t, abstract_device_t*>> plugin_devices,
             const std::vector<std::string>& args,
             std::vector<int> const hartids,
             const debug_module_config_t &dm_config,
             const char *log_path,
             bool dtb_enabled, const char *dtb_file) :
    start_pc(start_pc),
    htif_t(args),
    mems(mems),
    plugin_devices(plugin_devices),
//    procs(std::max(nprocs, size_t(1))), (modified 5)
    initrd_start(initrd_start),
    initrd_end(initrd_end),
    bootargs(bootargs),
    dtb_file(dtb_file ? dtb_file : ""),
    dtb_enabled(dtb_enabled),
    log_file(log_path),
    current_step(0),
    current_proc(0),
    debug(false),
    histogram_enabled(false),
    log(false),
    remote_bitbang(NULL),
    debug_module(this, dm_config)
{
  size_t nprocs = procs.size();

  signal(SIGINT, &handle_signal);

  if(argmap.find("mems=")==argmap.end()) {
    for (auto& x : mems)
      bus.add_device(x.first, x.second);
  }

  for (auto& x : plugin_devices)
    bus.add_device(x.first, x.second);

  debug_module.add_device(&bus);

  debug_mmu = new mmu_t(this, NULL);

  /*
  if (! (hartids.empty() || hartids.size() == nprocs)) {
      std::cerr << "Number of specified hartids ("
                << hartids.size()
                << ") doesn't match number of processors ("
                << nprocs << ").\n";
      exit(1);
  }

  // (modified 6)     !!!! this need in fesvr!!! 
  std::string isa_str = isa;
  if(htif_isa != "") 
    isa_str = htif_isa;
  for (size_t i = 0; i < procs.size(); i++) {
    int hart_id = hartids.empty() ? i : hartids[i];
    procs[i] = new processor_t(isa_str.c_str(), priv, varch, this, hart_id, halted,
                               log_file.get(),rstvec);
  }
  */
  //make_dtb();


  procs_init(hartids,(htif_isa == "")? isa : htif_isa.c_str(),priv,varch,halted);
  make_dtb();

  clint_init(real_time_clint);

  pmp_granularity_init();
  load_file();
}

void sim_t::pmp_granularity_init() 
{  
  #ifdef VF_DEBUG
  puts("############pmp init start############");
  #endif 
  size_t nprocs = procs.size();
  for (size_t i = 0; i < nprocs; i++) {
    reg_t pmp_num = 0, pmp_granularity = 0;
    fdt_parse_pmp_num((void *)dtb.c_str(), &pmp_num, "riscv");
    fdt_parse_pmp_alignment((void *)dtb.c_str(), &pmp_granularity, "riscv");

    #ifdef VF_DEBUG
      printf("    i:%zu,set_pmp_num:%zu,set_pmp_gran:%zu\n",i,pmp_num,pmp_granularity);
    #endif
    procs[i]->set_pmp_num(pmp_num);
    procs[i]->set_pmp_granularity(pmp_granularity);
  }
  #ifdef VF_DEBUG
  puts("############pmp init end############\n");
  #endif
  
}

void sim_t::clint_init(bool real_time_clint) 
{
  puts("############clint_t init start############");
  clint.reset(new clint_t(procs, CPU_HZ / INSNS_PER_RTC_TICK, real_time_clint));
  reg_t clint_base;
  if (fdt_parse_clint((void *)dtb.c_str(), &clint_base, "riscv,clint0")) {
    bus.add_device(CLINT_BASE, clint.get());
  } else {
    bus.add_device(clint_base, clint.get());
  }
  puts("############clint_t init end############\n");

}


void sim_t::make_dtb()
{ 
  //----dts generated from chip_config
  #ifdef VF_DEBUG
  printf("############make_dtb start############\n    INSNS_P_R_T=%zu,freq=%zu,initrd_start=%zu,initrd_end=%zu\n    bootargs=%s\n    procs.pc=",
  INSNS_PER_RTC_TICK,freq,initrd_start,initrd_end,bootargs);
  for(size_t i = 0;i < procs.size(); i++) 
    printf("%zu ",procs[i]->get_state()->pc);
  printf("\n    htif_mems=");
  for(auto &p : htif_mems) 
    printf("reg_t : %zu",p.first),PRINT_MEM_T_PTR(p.second); 
  puts("");
  #endif

  std::string dts = make_dts(INSNS_PER_RTC_TICK, freq/*CPU_HZ*/, initrd_start, initrd_end, bootargs, procs, htif_mems);
  //----building through dts

  #ifdef VF_DEBUG
  printf("    dts=%s,size=%zu\n",dts.c_str(),dts.size());
  #endif

  dtb = dts_compile(dts);

  #ifdef VF_DEBUG
  printf("    dtb=%s,size=%zu\n############make_dtb end############\n\n",dtb.c_str(),dtb.size());
  #endif
}
sim_t::~sim_t()
{
  for (size_t i = 0; i < procs.size(); i++)
    delete procs[i];
  delete debug_mmu;
}

void sim_thread_main(void* arg)
{
  ((sim_t*)arg)->main();
}

void sim_t::main()
{
  if (!debug && log)
    set_procs_debug(true);

  while (!done())
  {
    if (debug || ctrlc_pressed)
      interactive();
    else
      step(INTERLEAVE);
    if (remote_bitbang) {
      remote_bitbang->tick();
    }
  }
}

int sim_t::run()
{
  host = context_t::current();
  target.init(sim_thread_main, this);
  return htif_t::run();
}

void sim_t::step(size_t n)
{
  //puts("detectere sim_t::step");
  for (size_t i = 0, steps = 0; i < n; i += steps)
  {
    steps = std::min(n - i, INTERLEAVE - current_step);
    procs[current_proc]->step(steps);

    current_step += steps;
    if (current_step == INTERLEAVE)
    {
      current_step = 0;
      procs[current_proc]->get_mmu()->yield_load_reservation();
      if (++current_proc == procs.size()) {
        current_proc = 0;
        clint->increment(INTERLEAVE / INSNS_PER_RTC_TICK);
      }

      host->switch_to();
    }
  }
}

void sim_t::set_debug(bool value)
{
  debug = value;
}

void sim_t::set_histogram(bool value)
{
  histogram_enabled = value;
  for (size_t i = 0; i < procs.size(); i++) {
    procs[i]->set_histogram(histogram_enabled);
  }
}

void sim_t::configure_log(bool enable_log, bool enable_commitlog)
{
  log = enable_log;

  if (!enable_commitlog)
    return;

#ifndef RISCV_ENABLE_COMMITLOG
  fputs("Commit logging support has not been properly enabled; "
        "please re-build the riscv-isa-sim project using "
        "\"configure --enable-commitlog\".\n",
        stderr);
  abort();
#else
  for (processor_t *proc : procs) {
    proc->enable_log_commits();
  }
#endif
}


void sim_t::set_procs_debug(bool value)
{
  for (size_t i=0; i< procs.size(); i++)
    procs[i]->set_debug(value);
}

static bool paddr_ok(reg_t addr)
{
  if((addr >> MAX_PADDR_BITS) == 0) {
  //  printf("detecter paddr_ok ok\n");

  } else printf("detecter paddr_ok not ok  addr=0x%llx\n",addr);
  return (addr >> MAX_PADDR_BITS) == 0;
}

bool sim_t::mmio_load(reg_t addr, size_t len, uint8_t* bytes)
{
  //puts("detecter sim_t::mmioload");
  if (addr + len < addr || !paddr_ok(addr + len - 1))
    return false;
  return bus.load(addr, len, bytes);
}

bool sim_t::mmio_store(reg_t addr, size_t len, const uint8_t* bytes)
{
  //printf("detecter sim_t mmio_store addr=0x%llx,len=%zu,%p\n",addr,len,(uint8_t*)bytes);
  if (addr + len < addr || !paddr_ok(addr + len - 1)){
    //printf("detecter sim_t mmio store not ok");
    return false;
  }
  return bus.store(addr, len, bytes);
}

void sim_t::load_file() 
{
  #ifdef VF_DEBUG
  puts("############load_file start############");
  #endif
  std::string files_str = argmap["load_files="];
  std::vector<std::string> files;
  htif_helper_comma_separate(files_str,files);
  #define PAIR_STR_T std::pair<std::string,std::string>
  for(auto s:files){
    PAIR_STR_T dst;
    bool ret = htif_helper_underline_separate(s,dst.first,dst.second);
    assert(ret == 0); // ret == 1 <=> load_file err 
    size_t fsize = 0;
    addr_t addr = std::stoull(dst.second,0,16); 
    char* bin = htif_helper_get_file(dst.first,fsize);
    assert(bin!=nullptr);

    
    #ifdef VF_DEBUG
    printf("    s=%s\n    mem.write(addr=0x%llx,fsize=%zu,bin.get()=%p)\n",
          s.c_str(),addr,fsize,bin);//bin.get());
    #endif
    mem.write(from_le(addr),fsize,bin);//bin.get());
    #ifdef VF_DEBUG 
    puts("    mem.write committed######");
    #endif
    munmap(bin/*.get()*/,fsize);
  }
    //delete [] bin;
  #ifdef VF_DEBUG
  puts("############load_file end############\n\n");
  #endif
}

//(modified 4)
// outside input of this func: start_pc,dtb_file
void sim_t::set_rom()
{
  #ifdef VF_DEBUG
  puts("############make_rom start############");
  #endif 
  const int reset_vec_size = 8;
  
  //start_pc = start_pc == reg_t(-1) ? get_entry_point() : start_pc; // as parameter in htif

  uint32_t reset_vec[reset_vec_size] = {
    0x297,                                      // auipc  t0,0x0
    0x28593 + (reset_vec_size * 4 << 20),       // addi   a1, t0, &dtb
    0xf1402573,                                 // csrr   a0, mhartid
    get_core(0)->get_xlen() == 32 ?
      0x0182a283u :                             // lw     t0,24(t0)
      0x0182b283u,                              // ld     t0,24(t0)
    0x28067,                                    // jr     t0
    0,
    (uint32_t) (start_pc & 0xffffffff),
    (uint32_t) (start_pc >> 32)
  };
  for(int i = 0; i < reset_vec_size; i++)
    reset_vec[i] = to_le(reset_vec[i]);

  std::vector<char> rom((char*)reset_vec, (char*)reset_vec + sizeof(reset_vec));

  #ifdef VF_DEBUG
  #define PRINT_ROM(x) \
  puts(x);\
  printf("    "); \
  for(size_t i = 0; i < rom.size(); i++ )  \
    printf("%d ",rom[i]);\
  puts("|||");
  

  PRINT_ROM("add reset_vec to rom:\n")
  #endif

  
  /*
  std::string dtb;
  if (!dtb_file.empty()) {
    std::ifstream fin(dtb_file.c_str(), std::ios::binary);
    if (!fin.good()) {
      std::cerr << "can't find dtb file: " << dtb_file << std::endl;
      exit(-1);
    }

    std::stringstream strstream;
    strstream << fin.rdbuf();

    dtb = strstream.str();
  } else {
    dts = make_dts(INSNS_PER_RTC_TICK, CPU_HZ, initrd_start, initrd_end, bootargs, procs, mems);
    dtb = dts_compile(dts);
  }*/

  rom.insert(rom.end(), dtb.begin(), dtb.end());

  #ifdef VF_DEBUG
  PRINT_ROM("add dtb to rom:")
  #endif

  const int align = 0x1000;
  rom.resize((rom.size() + align - 1) / align * align);

  #ifdef VF_DEBUG
  PRINT_ROM("add align to rom:")
  #endif

  boot_rom.reset(new rom_device_t(rom));
  bus.add_device(rstvec, boot_rom.get());
  #ifdef VF_DEBUG
  printf("    commit result\n    rstvec=0x%llx,start_pc=0x%llx,boot_rom.get()=%p,rom.size()=0x%llx\n############make_rom end ############\n\n",rstvec,start_pc,boot_rom.get(),rom.size());

  
  #endif
}

// input dtb_file , output dtb string. 
// if dtb_file is empty, use current info to build one dts, then use dts to build dtb
/*
void sim_t::make_dtb() {
  if (!dtb_file.empty()) {
    std::ifstream fin(dtb_file.c_str(), std::ios::binary);
    if (!fin.good()) {
      std::cerr << "can't find dtb file: " << dtb_file << std::endl;
      exit(-1);
    }

    std::stringstream strstream;
    strstream << fin.rdbuf();

    dtb = strstream.str();
  } else {
    dts = make_dts(INSNS_PER_RTC_TICK, CPU_HZ, initrd_start, initrd_end, bootargs, procs, mems);
    dtb = dts_compile(dts);
  }
}*/

/*
void sim_t::set_rom()
{
  const int reset_vec_size = 8;

  start_pc = start_pc == reg_t(-1) ? get_entry_point() : start_pc; // as parameter in htif

  uint32_t reset_vec[reset_vec_size] = {
    0x297,                                      // auipc  t0,0x0
    0x28593 + (reset_vec_size * 4 << 20),       // addi   a1, t0, &dtb
    0xf1402573,                                 // csrr   a0, mhartid
    get_core(0)->get_xlen() == 32 ?
      0x0182a283u :                             // lw     t0,24(t0)
      0x0182b283u,                              // ld     t0,24(t0)
    0x28067,                                    // jr     t0
    0,
    (uint32_t) (start_pc & 0xffffffff),
    (uint32_t) (start_pc >> 32)
  };
  for(int i = 0; i < reset_vec_size; i++)
    reset_vec[i] = to_le(reset_vec[i]);

  std::vector<char> rom((char*)reset_vec, (char*)reset_vec + sizeof(reset_vec));

  std::string dtb;
  if (!dtb_file.empty()) {
    std::ifstream fin(dtb_file.c_str(), std::ios::binary);
    if (!fin.good()) {
      std::cerr << "can't find dtb file: " << dtb_file << std::endl;
      exit(-1);
    }

    std::stringstream strstream;
    strstream << fin.rdbuf();

    dtb = strstream.str();
  } else {
    dts = make_dts(INSNS_PER_RTC_TICK, CPU_HZ, initrd_start, initrd_end, bootargs, procs, mems);
    dtb = dts_compile(dts);
  }

  rom.insert(rom.end(), dtb.begin(), dtb.end());
  const int align = 0x1000;
  rom.resize((rom.size() + align - 1) / align * align);

  boot_rom.reset(new rom_device_t(rom));
  bus.add_device(DEFAULT_RSTVEC, boot_rom.get());
}*/

char* sim_t::addr_to_mem(reg_t addr) {
  if (!paddr_ok(addr))
    return NULL;
  auto desc = bus.find_device(addr);
  //printf("detecter addr:0x%llx,desc.fir:%llx,desc.sec:%p\n",addr,desc.first,desc.second);
  if (auto mem = dynamic_cast<mem_t*>(desc.second)) {
    //printf("detecter addr:0x%llx,desc.first:0x%llx,memsize:0x%llx\n",addr,desc.first,mem->size());
    if (addr - desc.first < mem->size())  {
      return mem->contents() + (addr - desc.first);
    }
  }
  
  //puts("detecter::addr_to_mem not ok");
  return NULL;
}

// htif

void sim_t::reset()
{
  if (dtb_enabled)
    set_rom();
}

void sim_t::idle()
{
  target.switch_to();
}

void sim_t::read_chunk(addr_t taddr, size_t len, void* dst)
{
  assert(len == 8);
  //printf("detecter sim_t::read_chunk taddr=0x%llx,len=0x%llx\n",taddr,len);
  auto data = to_le(debug_mmu->load_uint64(taddr));
  memcpy(dst, &data, sizeof data);
}

void sim_t::write_chunk(addr_t taddr, size_t len, const void* src)
{
  assert(len == 8);
  uint64_t data;
  memcpy(&data, src, sizeof data);
  //printf("detecter sim_t::write_chunk taddr=0x%llx,data_ptr=%p,data=%llx\n",taddr,&data,*(int64_t*)src);
  debug_mmu->store_uint64(taddr, from_le(data));
}

void sim_t::proc_reset(unsigned id)
{
  debug_module.proc_reset(id);
}
