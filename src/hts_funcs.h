//
// Created by Kez Cleal on 04/08/2022.
//

#pragma once

#include <string>
#include <vector>

#include "htslib/faidx.h"
#include "htslib/hfile.h"
#include "htslib/hts.h"
#include "htslib/vcf.h"
#include "htslib/sam.h"
#include "htslib/tbx.h"

#include "parser.h"
#include "../include/unordered_dense.h"
#include "segments.h"
#include "themes.h"


namespace HGW {

     enum FType {
         GFF3_IDX,
         BED_IDX,
         VCF_IDX,
         BCF_IDX,
         VCF_NOI,  // NOI for no index
         BED_NOI,
         GFF3_NOI,
         GW_LABEL,
         STDIN,
    };

    void guessRefGenomeFromBam(std::string &inputName, Themes::IniOptions &opts, std::vector<std::string> &bam_paths, std::vector<Utils::Region> &regions);

    void print_BCF_IDX(hts_idx_t *idx_v, bcf_hdr_t *hdr, std::string &chrom, int pos, htsFile *fp, std::string &id_str, std::string &variantString);

    void print_VCF_NOI(std::string &path, std::string &id_str, std::string &varinatString);

    void print_VCF_IDX(std::string &path, std::string &id_str, std::string &chrom, int pos, std::string &varinatString);

	void print_cached(std::vector<Utils::TrackBlock> &vals, std::string &chrom, int pos, bool flat, std::string &varinatString);

    /*
    * VCF or BCF file reader only. Cache's lines from stdin or non-indexed file. Can parse labels from file
    */
    class VCFfile {
    public:
        VCFfile () = default;
        ~VCFfile();
        htsFile *fp;
        bcf_hdr_t *hdr;
        hts_idx_t *idx_v;
        tbx_t *idx_t;
        std::vector<bcf1_t*> lines;
        bcf1_t *v;
        FType kind;
        std::string path;
        std::string chrom, chrom2, rid, vartype, label, tag;
        ankerl::unordered_dense::set<std::string> *seenLabels;
        int parse;
        int info_field_type;
        const char *label_to_parse;
        long start, stop;
        bool done;
        bool cacheStdin;
		bool samples_loaded;
		std::string variantString;
		std::vector<std::string> sample_names;

        void open(const std::string &f);
        void next();
        void printTargetRecord(std::string &id_str, std::string &chrom, int pos);
		void get_samples();

    };

    void collectReadsAndCoverage(Segs::ReadCollection &col, htsFile *bam, sam_hdr_t *hdr_ptr,
                                 hts_idx_t *index, int threads, Utils::Region *region,
                                 bool coverage, bool low_mem,
                                 std::vector<Parse::Parser> &filters, BS::thread_pool &pool);

    void iterDraw(std::vector<Segs::ReadCollection> &cols, int idx, htsFile *b, sam_hdr_t *hdr_ptr,
                        hts_idx_t *index, int threads, Utils::Region *region,
                        bool coverage, bool low_mem,
                        std::vector<Parse::Parser> &filters, Themes::IniOptions &opts, SkCanvas *canvas,
                        float trackY, float yScaling, Themes::Fonts &fonts, float refSpace);

    void trimToRegion(Segs::ReadCollection &col, bool coverage, int snp_threshold);

    void refreshLinked(std::vector<Segs::ReadCollection> &collections, Themes::IniOptions &opts, int *samMaxY);

    void appendReadsAndCoverage(Segs::ReadCollection &col, htsFile *bam, sam_hdr_t *hdr_ptr,
                                hts_idx_t *index, Themes::IniOptions &opts, bool coverage, bool left, int *samMaxY,
                                std::vector<Parse::Parser> &filters, BS::thread_pool &pool);

    /*
    * VCF/BCF/BED/GFF3/LABEL file reader. No label parsing for vcf/bcf.
    * Non-indexed files are cached using TrackBlock items. Files with an index are fetched during drawing.
    */
    class GwTrack {
    public:
        GwTrack() = default;
        ~GwTrack();
        // The iterator state is cached here during iteration:
        std::string path, genome_tag;
        std::string chrom, chrom2, rid, vartype, parent;
        int start, stop;
        int fileIndex;
        FType kind;

        htsFile *fp;
        tbx_t *idx_t;
        hts_idx_t *idx_v;
        bcf_hdr_t *hdr;

        bcf1_t *v;
        tbx_t *t;
        hts_itr_t *iter_q;

        int region_end;
        std::vector<std::string> parts;  // string split by delimiter
//        std::vector<Utils::TrackBlock> vals;
        std::vector<Utils::TrackBlock>::iterator vals_end;
        std::vector<Utils::TrackBlock>::iterator iter_blk;

        ankerl::unordered_dense::map< std::string, std::vector<Utils::TrackBlock>>  allBlocks;
        std::vector<Utils::TrackBlock> allBlocks_flat;
        Utils::TrackBlock block;
        bool done;
		std::string variantString;

        void open(const std::string &p, bool add_to_dict);
        void fetch(const Utils::Region *rgn);
        void next();
        bool findFeature(std::string &feature, Utils::Region &region);
        void parseVcfRecord(Utils::TrackBlock &b);
        void parseVcfRecord();
        void printTargetRecord(std::string &id_str, std::string &chrm, int pos);
    };

    bool searchTracks(std::vector<GwTrack> &tracks, std::string &feature, Utils::Region &region);

    void saveVcf(VCFfile &input_vcf, std::string path, std::vector<Utils::Label> multiLabels);


    /*
     * A union of VCFfile and GwTrack, tiled images will be drawn from this class
    */
    class GwVariantTrack {
    public:
        GwVariantTrack(std::string &path, bool cacheStdin, Themes::IniOptions *t_opts, int startIndex,
                       std::vector<std::string> &t_labelChoices,
                       ankerl::unordered_dense::map< std::string, Utils::Label> *t_inputLabels,
                       ankerl::unordered_dense::set<std::string> *t_seenLabels);
        ~GwVariantTrack();
        bool init;
        bool useVcf;
        bool *trackDone;
        int mouseOverTileIndex;
        int blockStart;
        std::string path;
        HGW::VCFfile vcf;
        HGW::GwTrack variantTrack;
        std::vector<std::vector<Utils::Region>> multiRegions;  // used for creating tiled regions
        std::vector<Utils::Label> multiLabels;  // used for labelling tiles
        std::vector<std::string> labelChoices;
        ankerl::unordered_dense::map< std::string, Utils::Label> *inputLabels;
        Themes::IniOptions *m_opts;

        void nextN(int number);
        void iterateToIndex(int index);

    private:
        void appendVariantSite(std::string &chrom, long start, std::string &chrom2, long stop, std::string &rid, std::string &label, std::string &vartype);
    };

    void collectGFFTrackData(HGW::GwTrack &trk, std::vector<Utils::TrackBlock> &features);

    void collectTrackData(HGW::GwTrack &trk, std::vector<Utils::TrackBlock> &features);

}
