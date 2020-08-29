#include<getopt.h>
#include<vector>
#include<iostream>
#include<string>
#include<cstdio>
#include<unordered_map>
//(unit test)
// this test file's objective is to find out 
// info about gnu function of getopt
// Also is considered as unit test of htif.cc:parse_argument

using namespace std;
#define HTIF_LONG_OPTIONS_OPTIND 1024
#define HTIF_LONG_OPTIONS                                               \
{"help",      no_argument,       0, 'h'                          },     \
{"rfb",       optional_argument, 0, HTIF_LONG_OPTIONS_OPTIND     },     \
{"disk",      required_argument, 0, HTIF_LONG_OPTIONS_OPTIND + 1 },     \
{"signature", required_argument, 0, HTIF_LONG_OPTIONS_OPTIND + 2 },     \
{"chroot",    required_argument, 0, HTIF_LONG_OPTIONS_OPTIND + 3 },     \
{"payload",   required_argument, 0, HTIF_LONG_OPTIONS_OPTIND + 4 },     \
{0, 0, 0, 0}
#define PLUS_PLUS_SEMANTIC 2048
vector<string> targs;
const unordered_map<std::string,bool> is_keyword = {
  {"load_pk",1},
  {"load_rom",1},
  {"chrome_rom",1},
  {"load_files",1},
  {"chip_config",1},
  {"shift_file",1},
  {"mems",1},
  {"normal_load",1},
  {"load_to_flash",1}
};

int main(int argc, char ** argv) {
  optind = 0; // reset optind as HTIF may run getopt _after_ others
  std::string plus_plus_buf="";
  while (1) {
    static struct option long_options[] = { HTIF_LONG_OPTIONS };
    int option_index = 0;
    int c = getopt_long(argc, argv, "-h", long_options, &option_index);
    printf("c=%d\n",c);

    if (c == -1) break;
 retry:
    switch (c) {
      case 'h': 
        puts("case=h");
        break;
        //usage(argv[0]);
        //throw std::invalid_argument("User queried htif_t help text");
      case HTIF_LONG_OPTIONS_OPTIND:
        puts("HTIF_LONG_OPTIONS_POTIND");
        //if (optarg) dynamic_devices.push_back(new rfb_t(atoi(optarg)));
        //else        dynamic_devices.push_back(new rfb_t);
        break;
      case HTIF_LONG_OPTIONS_OPTIND + 1:
        // [TODO] Remove once disks are supported again
        puts("HTIF_LONG_OPTIONS_POTIND+1");
        //throw std::invalid_argument("--disk/+disk unsupported (use a ramdisk)");
        //dynamic_devices.push_back(new disk_t(optarg));
        break;
      case HTIF_LONG_OPTIONS_OPTIND + 2:
        puts("HTIF_LONG_OPTIONS_POTIND+2");
        //sig_file = optarg;
        break;
      case HTIF_LONG_OPTIONS_OPTIND + 3:
        puts("HTIF_LONG_OPTIONS_POTIND+3");
        //syscall_proxy.set_chroot(optarg);
        break;
      case HTIF_LONG_OPTIONS_OPTIND + 4:
        puts("HTIF_LONG_OPTIONS_POTIND+4");
        //payloads.push_back(optarg);
        break;
      case PLUS_PLUS_SEMANTIC://(modified)
        if(*optarg==0 &&is_keyword.find(plus_plus_buf) == is_keyword.end()) {
          // case of such as ++../bbl/build/bbl0 loading a file
          printf("plusplusbuf=%s,optarg=%s,optarg==\"\"?=%d\n","normal_load" ,plus_plus_buf.c_str(),plus_plus_buf == string("") ); 
          //argmap["normal_load"] = plus_plus_buf; 
        } else {
          // optarg is empty string {'\000'} if no value arg there
          printf("plusplusbuf=%s,optarg=%s,optarg==\"\"?=%d\n",plus_plus_buf.c_str() ,string(optarg).c_str(),optarg == string("") ); // optarg is empty string {'\000'} if no value arg there
          //argmap[plus_plus_buf] = optarg; 
        }
        break;
        
        
        break;  
      case '?':
        puts("?");
        if (!opterr) {
          puts("!opterr");
          break;
        }
        break;
        //throw std::invalid_argument("Unknown argument (did you mean to enable +permissive parsing?)");
      case 1: {
        std::string arg = optarg;
        printf("arg=%s\n",arg.c_str());
        if (arg == "+h" || arg == "+help") {
          c = 'h';
          optarg = nullptr;
          puts("+h or +help");
        }
        else if (arg == "+rfb") {
          c = HTIF_LONG_OPTIONS_OPTIND;
          optarg = nullptr;
          puts("==+rfb");
        }
        else if (arg.find("+rfb=") == 0) {
          c = HTIF_LONG_OPTIONS_OPTIND;
          optarg = optarg + 5;
          puts("+rfb=");
        }
        else if (arg.find("+disk=") == 0) {
          c = HTIF_LONG_OPTIONS_OPTIND + 1;
          optarg = optarg + 6;
          puts("+disk=");
        }
        else if (arg.find("+signature=") == 0) {
          c = HTIF_LONG_OPTIONS_OPTIND + 2;
          optarg = optarg + 11;
          puts("+signature=");
        }
        else if (arg.find("+chroot=") == 0) {
          c = HTIF_LONG_OPTIONS_OPTIND + 3;
          optarg = optarg + 8;
          puts("+chroot=");
        }
        else if (arg.find("+payload=") == 0) {
          c = HTIF_LONG_OPTIONS_OPTIND + 4;
          optarg = optarg + 9;
          puts("+payload=");
        }
        else if (arg.find("+permissive-off") == 0) {
          puts("+permissive-off");
          if (opterr){
              puts("opterr112");
              break;
           // throw std::invalid_argument("Found +permissive-off when not parsing permissively");
          }
          opterr = 1;
          break;
        }
        else if (arg.find("+permissive") == 0) {
          puts("+permissive");
          if (!opterr){
              puts("opterr121");
            //throw std::invalid_argument("Found +permissive when already parsing permissively");
          }
          opterr = 0;
          break;
        }       
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
          if (!opterr) {
            puts("opterr130");
            break;
          }
          else {
            optind--;
            puts("optind--");
            goto done_processing;
          }
        }
        goto retry;
      }
    }
  }

done_processing:
  printf("argc=%d optind=%d\n",argc,optind);
  puts("targs=");
  while (optind < argc) {
    targs.push_back(argv[optind++]);
    printf("%s ",targs[targs.size()-1].c_str());
  }
  puts("");
  if (!targs.size()) {
    //usage(argv[0]);
    //throw std::invalid_argument("No binary specified (Did you forget it? Did you forget '+permissive-off' if running with +permissive?)");
    puts("err targs.size()==0");
  }

}



