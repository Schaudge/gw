#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <deque>
#include <iostream>
#include <vector>
//#inc <thread>
#include "../inc/BS_thread_pool.h"
#include "htslib/sam.h"


//const int NORMAL = 0;
//const int DEL = 200;
//const int INV_F = 400;
//const int INV_R = 600;
//const int DUP = 800;
//const int TRA = 1000;
//const int SC = 2000;
//const int INS_f = 5000;
//const int INS_s = 6000;

namespace Segs {

    enum Pattern {
        NORMAL,
        DEL,
        INV_F,
        INV_R,
        DUP,
        TRA,
    };


    typedef int64_t hts_pos_t;


    struct InsItem {
        uint32_t pos, length;
    };


    struct QueueItem {
        uint32_t c_s_idx, l;
    };


    struct MMbase {
        uint32_t idx, pos;
    };


    struct MdBlock {
        uint32_t matches, md_idx, del_length;
        bool is_mm;
    };


    void get_md_block(char *md_tag, int md_idx, int md_l, MdBlock *res);


    void get_mismatched_bases(std::vector<MMbase> &result, char *md_tag, uint32_t r_pos, uint32_t ct_l, uint32_t *cigar_p);


    struct Align {
        bam1_t *delegate;

        uint32_t cov_start, cov_end, orient_pattern, left_soft_clip, right_soft_clip, left_hard_clip;
        float polygon_height;
        bool has_SA, has_NM, has_MD, initialized;
        int NM, y, edge_type;
        char *MD;

        uint32_t pos, reference_end, cigar_l;
        std::vector<uint32_t> block_starts, block_ends;
        std::vector<InsItem> any_ins;
        std::vector<MMbase> mismatches;
    };


    class ReadCollection {
    public:
        ReadCollection() {};
        ~ReadCollection() = default;

        std::vector<int> covArr, levelsStart, levelsEnd;
        std::vector<Align> readQueue;
    };


    void align_init(Align *self);

    void init_parallel(std::vector<Align> *aligns, int n);

}