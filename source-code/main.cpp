//
//  main.cpp
//  lab2
//
//  Created by 郜瑞啸 on 2020/4/4.
//  Copyright © 2020 郜瑞啸. All rights reserved.
//
#include <cstdio>
#include <elfio/elfio.hpp>
#include "MemoryManager.hpp"
#include "BranchPredictor.hpp"
#include "Simulator.hpp"
#include "Cache.hpp"


using std::string;

bool verbose = false;
bool single_step = false;
bool dump_history = false;
char *elf_file_name = nullptr;
Cache *l1_cache, *l2_cache, *l3_cache;
BranchPredictor::Strategy strategy = BranchPredictor::Strategy::NT;


bool parsePara(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
                case 'v':
                    verbose = true;
                    break;
                case 's':
                    single_step = true;
                    break;
                case 'd':
                    dump_history = true;
                    break;
                case 'b':
                    if (i + 1 < argc) {
                        string branch = argv[++i];
                        if (branch == "AT") {
                            strategy = BranchPredictor::Strategy::AT;
                        } else if (branch == "NT") {
                            strategy = BranchPredictor::Strategy::NT;
                        } else if (branch == "BTFNT") {
                            strategy = BranchPredictor::Strategy::BTFNT;
                        } else if (branch == "BPB") {
                            strategy = BranchPredictor::Strategy::BPB;
                        } else {
                            return false;
                        }
                    } else {
                        return false;
                    }
                    break;
                default:
                    return false;
            }
        } else {
            elf_file_name = argv[i];
        }
    }

    return elf_file_name != nullptr;
}

void printUsage() {
    printf("Usage: ./Sim riscv-elf-file-name [-v] [-d] [-s] [-b para]\n");
    printf("\tParameters:\n");
    printf("\t\t[-v]: to use verbose mode\n");
    printf("\t\t[-d]: to dump memory and reg\n");
    printf("\t\t[-s]: to enter single step mode\n");
    printf("\t\t[-b para]: claim branch perdiction strategy\n");
    printf("\t\t           accpeted para: AT, NT, BTFNT, BPB\n");
    printf("\t\t           AT:    Always Taken\n");
    printf("\t\t           NT:    Always Not Taken\n");
    printf("\t\t           BTFNT: Backword Taken, Forward Not Taken\n");
    printf("\t\t           BPB:   Branch Prediction Buffer\n");
}

void loadElf(ELFIO::elfio *reader, MemoryManager *memory) {
    ELFIO::Elf_Half seg_num = reader->segments.size();
    if (verbose) {
        printf("-------load ELF-------\n");
    }

//    printf("Seg Num:%x\n", seg_num);

    for (uint32_t i = 0; i < seg_num; i++) {
        const ELFIO::segment *seg_pointer = reader->segments[i];
        uint64_t full_mem = seg_pointer->get_memory_size();
        uint64_t full_addr = seg_pointer->get_virtual_address();


//        printf("%llx %llx\n", full_mem, full_addr);
        if (full_addr + full_mem > 0xFFFFFFFF) {
            printf("ELF address space is larger than 32bit!\n");
            printf("Still could not deal with it!\n");
            exit(-1);
        }

        uint32_t file_size = (uint32_t) seg_pointer->get_file_size();
        uint32_t mem_size = (uint32_t) full_mem;
        uint32_t addr = (uint32_t) full_addr;

//        printf("%x %x %x\n", file_size, mem_size, addr);

        for (uint32_t pos = addr; pos < addr + mem_size; pos++) {
            if (!memory->pageExist(pos)) {
                memory->addPage(pos);
            }
            if (pos < addr + file_size) {
                memory->setByteNoCache(pos, seg_pointer->get_data()[pos - addr]);
            } else {
                memory->setByteNoCache(pos, 0);
            }
        }


    }
    if (verbose) {
        printf("-----------------------\n");
    }
}


int main(int argc, char **argv) {
    if (!parsePara(argc, argv)) {
        printUsage();
        exit(-1);
    }
    MemoryManager memory;
    Cache::Policy policy;

    policy.cache_size = 8 * 1024 * 1024;
    policy.block_size = 64;
    policy.block_num = policy.cache_size / policy.block_size;
    policy.associativity = 8;
    policy.hit_latency = 19;
//    policy.miss_latency = 100;
    l3_cache = new Cache(&memory, policy, nullptr, true, true);

    policy.cache_size = 256 * 1024;
    policy.block_size = 64;
    policy.block_num = policy.cache_size / policy.block_size;
    policy.associativity = 8;
    policy.hit_latency = 7;
//    policy.miss_latency = 20;
    l2_cache = new Cache(&memory, policy, l3_cache, true, true);

    policy.cache_size = 32 * 1024;
    policy.block_size = 64;
    policy.block_num = policy.cache_size / policy.block_size;
    policy.associativity = 8;
    policy.hit_latency = 0;
//    policy.miss_latency = 8;
    l1_cache = new Cache(&memory, policy, l2_cache, true, true);

    memory.setCache(l1_cache);

    BranchPredictor branch_predictor(strategy);
    Simulator simulator(&memory, &branch_predictor);

    ELFIO::elfio reader;
    if (!reader.load(elf_file_name)) {
        fprintf(stderr, "Failure on loading ELF File\n");
        exit(-1);
    }

    loadElf(&reader, &memory);

    if (verbose) {
//        printELFInfo(&reader);
        memory.printInfo();
    }

    simulator.is_single_step = single_step;
    simulator.is_verbose = verbose;
    simulator.is_dump_history = dump_history;
    simulator.initStack(0x80000000, 0x400000);
//    printf("%d %d %d %d\n",single_step, verbose, dump_history, strategy);
    uint64_t pc = reader.get_entry();
//    printf("Entry: %llx\n", pc);
    simulator.run(reader.get_entry());

//    if(dump_history){
//        simulator.dumpHistory();
//    }
    delete l1_cache;
    delete l2_cache;
    delete l3_cache;

    return 0;
}
