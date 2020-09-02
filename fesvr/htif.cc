// See LICENSE for license details.

#include "htif.h"
#include "rfb.h"
#include "elfloader.h"
#include "encoding.h"
#include "byteorder.h"
#include <algorithm>
#include <assert.h>
#include <vector>
#include <queue>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>

/* Attempt to determine the execution prefix automatically.  autoconf
 * sets PREFIX, and pconfigure sets __PCONFIGURE__PREFIX. */
#if !defined(PREFIX) && defined(__PCONFIGURE__PREFIX)
# define PREFIX __PCONFIGURE__PREFIX
#endif

#ifndef TARGET_ARCH
# define TARGET_ARCH "riscv64-unknown-elf"
#endif

#ifndef TARGET_DIR
# define TARGET_DIR "/" TARGET_ARCH "/bin/"
#endif

static volatile bool signal_exit = false;
static void handle_signal(int sig)
{
  if (sig == SIGABRT || signal_exit) // someone set up us the bomb!
    exit(-1);
  signal_exit = true;
  signal(sig, &handle_signal);
}

htif_t::htif_t()
  : mem(this), entry(DRAM_BASE), sig_addr(0), sig_len(0),
    tohost_addr(0), fromhost_addr(0), exitcode(0), stopped(false),
    syscall_proxy(this)
{
  signal(SIGINT, &handle_signal);
  signal(SIGTERM, &handle_signal);
  signal(SIGABRT, &handle_signal); // we still want to call static destructors
}

htif_t::htif_t(int argc, char** argv) : htif_t()
{
  parse_arguments(argc, argv);
  register_devices();
}

htif_t::htif_t(const std::vector<std::string>& args) : htif_t()
{
  int argc = args.size() + 1;
  char * argv[argc];
  argv[0] = (char *) "htif";
  for (unsigned int i = 0; i < args.size(); i++) {
    argv[i+1] = (char *) args[i].c_str();
  }

  parse_arguments(argc, argv);
  register_devices();
}

htif_t::~htif_t()
{
  for (auto d : dynamic_devices)
    delete d;
}

void htif_t::start()
{
  if (!targs.empty() && targs[0] != "none")
      load_program();

  reset();
}

std::map<std::string, uint64_t> htif_t::load_payload(const std::string& payload, reg_t* entry)
{
  std::string path;
  if (access(payload.c_str(), F_OK) == 0)
    path = payload;
  else if (payload.find('/') == std::string::npos)
  {
    std::string test_path = PREFIX TARGET_DIR + payload;
    if (access(test_path.c_str(), F_OK) == 0)
      path = test_path;
  }

  if (path.empty())
    throw std::runtime_error(
        "could not open " + payload +
        " (did you misspell it? If VCS, did you forget +permissive/+permissive-off?)");

  // temporarily construct a memory interface that skips writing bytes
  // that have already been preloaded through a sideband
  class preload_aware_memif_t : public memif_t {
   public:
    preload_aware_memif_t(htif_t* htif) : memif_t(htif), htif(htif) {}

    void write(addr_t taddr, size_t len, const void* src) override
    {
      if (!htif->is_address_preloaded(taddr, len))
        memif_t::write(taddr, len, src);
    }

   private:
    htif_t* htif;
  } preload_aware_memif(this);

  // (modified 3)
  reg_t shift_value = 0;
  if(htif_helper_bbl0_recognizer(path)) {
    shift_value = std::stoull(argmap["shift_file="]);
  }

  return load_elf(path.c_str(), &preload_aware_memif, entry, shift_value);
}

void htif_t::load_program()
{
  std::map<std::string, uint64_t> symbols = load_payload(targs[0], &entry);

  if (symbols.count("tohost") && symbols.count("fromhost")) {
    tohost_addr = symbols["tohost"];
    fromhost_addr = symbols["fromhost"];
  } else {
    fprintf(stderr, "warning: tohost and fromhost symbols not in ELF; can't communicate with target\n");
  }

  // detect torture tests so we can print the memory signature at the end
  if (symbols.count("begin_signature") && symbols.count("end_signature"))
  {
    sig_addr = symbols["begin_signature"];
    sig_len = symbols["end_signature"] - sig_addr;
  }

  for (auto payload : payloads)
  {
    reg_t dummy_entry;
    load_payload(payload, &dummy_entry);
  }
}

void htif_t::stop()
{
  if (!sig_file.empty() && sig_len) // print final torture test signature
  {
    std::vector<uint8_t> buf(sig_len);
    mem.read(sig_addr, sig_len, &buf[0]);

    std::ofstream sigs(sig_file);
    assert(sigs && "can't open signature file!");
    sigs << std::setfill('0') << std::hex;

    const addr_t incr = 16;
    assert(sig_len % incr == 0);
    for (addr_t i = 0; i < sig_len; i += incr)
    {
      for (addr_t j = incr; j > 0; j--)
        sigs << std::setw(2) << (uint16_t)buf[i+j-1];
      sigs << '\n';
    }

    sigs.close();
  }

  stopped = true;
}

void htif_t::clear_chunk(addr_t taddr, size_t len)
{
  char zeros[chunk_max_size()];
  memset(zeros, 0, chunk_max_size());

  for (size_t pos = 0; pos < len; pos += chunk_max_size())
    write_chunk(taddr + pos, std::min(len - pos, chunk_max_size()), zeros);
}

int htif_t::run()
{
  start();

  auto enq_func = [](std::queue<reg_t>* q, uint64_t x) { q->push(x); };
  std::queue<reg_t> fromhost_queue;
  std::function<void(reg_t)> fromhost_callback =
    std::bind(enq_func, &fromhost_queue, std::placeholders::_1);

  if (tohost_addr == 0) {
    while (true)
      idle();
  }

  while (!signal_exit && exitcode == 0)
  {
    if (auto tohost = from_le(mem.read_uint64(tohost_addr))) {
      mem.write_uint64(tohost_addr, 0);
      command_t cmd(mem, tohost, fromhost_callback);
      device_list.handle_command(cmd);
    } else {
      idle();
    }

    device_list.tick();

    if (!fromhost_queue.empty() && mem.read_uint64(fromhost_addr) == 0) {
      mem.write_uint64(fromhost_addr, to_le(fromhost_queue.front()));
      fromhost_queue.pop();
    }
  }

  stop();

  return exit_code();
}

bool htif_t::done()
{
  return stopped;
}

int htif_t::exit_code()
{
  return exitcode >> 1;
}

//(modified 5)
// objective: get dtb string of htif
// using argmap[chip_config=] to make dtb through dts compiling way
// dtb_file way of getting dtb is abandon here.

void htif_t::make_dtb()
{ 
  //----parsing the chip_config----
  auto &tmp = argmap["chip_config="];
  for(int i=0; i<tmp.length(); i++) {
    int j , contd = 0;
    for(j = i ; j<tmp.length() ; j++)  {
      if(tmp[j]=='_') {
        contd ++;
        if(contd == 2) {
          j ++;
          break;
        }
      }
    }

    i = j;
  }
  //----dts generated from chip_config
  std::string dts = make_dts(INSNS_PER_RTC_TICK, CPU_HZ, initrd_start, initrd_end, bootargs, procs, mems);
  //----building through dts
  dtb = dts_compile(dts);
}
//(modified )
void htif_t::load_rom() {
  make_dtb();
}

//(modified 4)
// outside input of this func: start_pc,dtb_file
void htif_t::set_rom(reg_t start_pc)
{
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
}




/*
parse_arguments can reconize HTIF_LONG_OPTIONS
in -- and + forms. If meets a -- type, exec flow
go through switch (c) 'h','1024---1028'. If meets
a + type, exec flow go through switch (c) '1' and 
go retry or go done_processing.

We add the semantic of ++ type, which
states a pair, for example
    ++load_file=../bbl0
then:
    argmap[load_file=]==../bbl0
*/

void htif_t::parse_arguments(int argc, char ** argv)
{
  optind = 0; // reset optind as HTIF may run getopt _after_ others
  std::string plus_plus_buf = "";
  while (1) {
    static struct option long_options[] = { HTIF_LONG_OPTIONS };
    int option_index = 0;
    int c = getopt_long(argc, argv, "-h", long_options, &option_index);

    if (c == -1) break;
 retry:
    switch (c) {
      case 'h': usage(argv[0]);
        throw std::invalid_argument("User queried htif_t help text");
      case HTIF_LONG_OPTIONS_OPTIND:
        if (optarg) dynamic_devices.push_back(new rfb_t(atoi(optarg)));
        else        dynamic_devices.push_back(new rfb_t);
        break;
      case HTIF_LONG_OPTIONS_OPTIND + 1:
        // [TODO] Remove once disks are supported again
        throw std::invalid_argument("--disk/+disk unsupported (use a ramdisk)");
        dynamic_devices.push_back(new disk_t(optarg));
        break;
      case HTIF_LONG_OPTIONS_OPTIND + 2:
        sig_file = optarg;
        break;
      case HTIF_LONG_OPTIONS_OPTIND + 3:
        syscall_proxy.set_chroot(optarg);
        break;
      case HTIF_LONG_OPTIONS_OPTIND + 4:
        payloads.push_back(optarg);
        break;
      case PLUS_PLUS_SEMANTIC://(modified 2)
        if(*optarg==0 && is_keyword.find(plus_plus_buf) == is_keyword.end()) {
          // case of such as ++../bbl/build/bbl0 loading a file
          argmap["normal_load"] = plus_plus_buf; 
        } else {
          // optarg is empty string {'\000'} if no value arg there
          argmap[plus_plus_buf] = optarg; 
        }
        break;
      case '?':
        if (!opterr)
          break;
        throw std::invalid_argument("Unknown argument (did you mean to enable +permissive parsing?)");
      case 1: {
        std::string arg = optarg;
        if (arg == "+h" || arg == "+help") {
          c = 'h';
          optarg = nullptr;
        }
        else if (arg == "+rfb") {
          c = HTIF_LONG_OPTIONS_OPTIND;
          optarg = nullptr;
        }
        else if (arg.find("+rfb=") == 0) {
          c = HTIF_LONG_OPTIONS_OPTIND;
          optarg = optarg + 5;
        }
        else if (arg.find("+disk=") == 0) {
          c = HTIF_LONG_OPTIONS_OPTIND + 1;
          optarg = optarg + 6;
        }
        else if (arg.find("+signature=") == 0) {
          c = HTIF_LONG_OPTIONS_OPTIND + 2;
          optarg = optarg + 11;
        }
        else if (arg.find("+chroot=") == 0) {
          c = HTIF_LONG_OPTIONS_OPTIND + 3;
          optarg = optarg + 8;
        }
        else if (arg.find("+payload=") == 0) {
          c = HTIF_LONG_OPTIONS_OPTIND + 4;
          optarg = optarg + 9;
        }
        else if (arg.find("+permissive-off") == 0) {
          if (opterr)
            throw std::invalid_argument("Found +permissive-off when not parsing permissively");
          opterr = 1;
          break;
        }
        else if (arg.find("+permissive") == 0) {
          if (!opterr)
            throw std::invalid_argument("Found +permissive when already parsing permissively");
          opterr = 0;
          break;
        } 
        // (modified 2)
        else if (arg.find("++") == 0) {
          size_t equ_pos = arg.find("=");
          if(std::string::npos != equ_pos) 
            plus_plus_buf = arg.substr(2,equ_pos-1),
            optarg += equ_pos + 1;
          else 
            plus_plus_buf = arg.substr(2,arg.size()-2),
            optarg += arg.size();
          c = PLUS_PLUS_SEMANTIC;
        }
        else {
          if (!opterr)
            break;
          else {
            optind--;
            goto done_processing;
          }
        }
        goto retry;
      }
    }
  }

done_processing:
  while (optind < argc)
    targs.push_back(argv[optind++]);
  if (!targs.size()) {
    usage(argv[0]);
    throw std::invalid_argument("No binary specified (Did you forget it? Did you forget '+permissive-off' if running with +permissive?)");
  }
}

void htif_t::register_devices()
{
  device_list.register_device(&syscall_proxy);
  device_list.register_device(&bcd);
  for (auto d : dynamic_devices)
    device_list.register_device(d);
}

void htif_t::usage(const char * program_name)
{
  printf("Usage: %s [EMULATOR OPTION]... [VERILOG PLUSARG]... [HOST OPTION]... BINARY [TARGET OPTION]...\n ",
         program_name);
  fputs("\
Run a BINARY on the Rocket Chip emulator.\n\
\n\
Mandatory arguments to long options are mandatory for short options too.\n\
\n\
EMULATOR OPTIONS\n\
  Consult emulator.cc if using Verilator or VCS documentation if using VCS\n\
    for available options.\n\
EMUALTOR VERILOG PLUSARGS\n\
  Consult generated-src*/*.plusArgs for available options\n\
", stdout);
  fputs("\n" HTIF_USAGE_OPTIONS, stdout);
}
