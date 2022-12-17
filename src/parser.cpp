//
// Created by Kez Cleal on 11/11/2022.
//
#include <algorithm>
#include <iostream>
#include <utility>
#include <string>
#include <regex>

#include <htslib/sam.h>

#include "../include/termcolor.h"
#include "utils.h"
#include "parser.h"
#include "segments.h"




namespace Parse {

    constexpr std::string_view numeric_like = "eq ne gt lt ge le == != > < >= <=";
    constexpr std::string_view string_like = "eq ne contains == != omit";

    Parser::Parser() {
        opMap["mapq"] = MAPQ;
        opMap["flag"] = FLAG;
        opMap["~flag"] = NFLAG;
        opMap["qname"] = QNAME;
        opMap["tlen"] = TLEN;
        opMap["abs-tlen"] = ABS_TLEN;
        opMap["rnext"] = RNEXT;
        opMap["pos"] = POS;
        opMap["ref-end"] = REF_END;
        opMap["pnext"] = PNEXT;
        opMap["seq"] = SEQ;
        opMap["seq-len"] = SEQ_LEN;

        opMap["RG"] = RG;
        opMap["BC"] = BC;
        opMap["LB"] = LB;
        opMap["MD"] = MD;
        opMap["MI"] = MI;
        opMap["PU"] = PU;
        opMap["SA"] = SA;
        opMap["MC"] = MC;

        opMap["NM"] = NM;
        opMap["CM"] = CM;
        opMap["FI"] = FI;
        opMap["HO"] = HO;
        opMap["MQ"] = MQ;
        opMap["SM"] = SM;
        opMap["TC"] = TC;
        opMap["UQ"] = UQ;
        opMap["AS"] = AS;
        opMap["BX"] = BX;
        opMap["RX"] = RX;

        opMap["eq"] = EQ;
        opMap["ne"] = NE;
        opMap["gt"] = GT;
        opMap["lt"] = LT;
        opMap["ge"] = GE;
        opMap["le"] = LE;
        opMap["=="] = EQ;
        opMap["!="] = NE;
        opMap[">"] = GT;
        opMap["<"] = LT;
        opMap[">="] = GE;
        opMap["<="] = LE;
        opMap["contains"] = CONTAINS;
        opMap["omit"] = OMIT;
        opMap["&"] = AND;

        opMap["paired"] = PAIRED;
        opMap["proper-pair"] = PROPER_PAIR;
        opMap["unmap"] = UNMAP;
        opMap["munmap"] = MUNMAP;
        opMap["reverse"] = REVERSE;
        opMap["mreverse"] = MREVERSE;
        opMap["read1"] = READ1;
        opMap["read2"] = READ2;
        opMap["secondary"] = SECONDARY;
        opMap["qcfail"] = QCFAIL;
        opMap["dup"] = DUP;
        opMap["supplementary"] = SUPPLEMENTARY;

        permit[MAPQ] = numeric_like;
        permit[FLAG] = "&";
        permit[NFLAG] = "&";
        permit[QNAME] = string_like;
        permit[TLEN] = numeric_like;
        permit[ABS_TLEN] = numeric_like;
        permit[POS] = numeric_like;
        permit[REF_END] = numeric_like;
        permit[PNEXT] = numeric_like;
        permit[RNEXT] = string_like;
        permit[SEQ] = string_like;
        permit[SEQ_LEN] = numeric_like;

        // tags
        permit[RG] = string_like;
        permit[BC] = string_like;
        permit[LB] = string_like;
        permit[MD] = string_like;
        permit[MI] = string_like;
        permit[PU] = string_like;
        permit[SA] = string_like;
        permit[MC] = string_like;
        permit[BX] = string_like;
        permit[RX] = string_like;
        permit[NM] = numeric_like;
        permit[CM] = numeric_like;
        permit[FI] = numeric_like;
        permit[HO] = numeric_like;
        permit[MQ] = numeric_like;
        permit[SM] = numeric_like;
        permit[TC] = numeric_like;
        permit[UQ] = numeric_like;
        permit[AS] = numeric_like;
    }

    int parse_indexing(std::string &s, int nBams, int nRegions, std::vector< std::vector<int> > &v) {
        // check for indexing. Makes a lookup table which describes which panels a filter should be applied to
        std::string::iterator iStart = s.end();
        std::string::iterator iEnd = s.end();
        bool open = false;
        bool close = false;
        while (iStart != s.begin()) {
            --iStart;
            if (*iStart == ' ') {
                continue;
            } else if (!open && *iStart == ']') {
                open = true;
                iEnd = iStart;
            } else if (open && *iStart == '[') {
                close = true;
                ++iStart;
                break;
            } else if (!open) {
                break;
            }
        }
        if (!open) {
            return 0;
        }
        if (!open != !close) {  // xor
            std::cerr << "Error: expression not understood: " << s << std::endl;
            return -1;
        }
        auto indexStr = std::string(iStart, iEnd);
        --iStart;
        s.erase(iStart, s.end());
        if (indexStr == ":") {
            return 1;
        }
        if (nBams == 0 || nRegions == 0) {
            std::cerr << "Error: No bam/region to filter. Trying to apply a filter to nBams==" << nBams << " and nRegions==" << nRegions << std::endl;
            return -1;
        }
        std::string lhs, rhs;
        indexStr.erase(std::remove(indexStr.begin(), indexStr.end(), ' '), indexStr.end());
        std::string::iterator itr = indexStr.begin();
        iStart = indexStr.begin();
        while (itr != indexStr.end()) {
            if (*itr == ',') {
                lhs = std::string(iStart, itr);
                iStart = itr; ++iStart;
                ++itr;
                continue;
            }
            ++itr;
        }
        rhs = std::string(iStart, itr);
        if (nBams > 1 && lhs.empty()) {
            std::cerr << "Error: if multiple bams are present you need to specify the [row,column] e.g. [:, 0] or [0,1] etc\n";
            return -1;
        }
        bool allRows = lhs == ":";
        bool allColumns = rhs == ":";
        int iRow, iCol;
        try {
            iRow = (lhs.empty()) ? 0 : (allRows) ? -1 : std::stoi(lhs);
            iCol = (allColumns) ? 0 : std::stoi(rhs);
        } catch (...) {
            std::cerr << "Error: string to integer failed for left-hand side=" << lhs << ", or right-hand side=" << rhs << std::endl;
            return -1;
        }

        if (std::abs(iRow) >= nBams) {
            std::cerr << "Error: row index is > nBams\n";
            return -1;
        }
        if (std::abs(iCol) >= nRegions) {
            std::cerr << "Error: column index is > nRegions\n";
            return -1;
        }
        iRow = (iRow < 0) ? nBams + iRow : iRow;  // support negative indexing
        iCol = (iCol < 0) ? nRegions + iCol : iCol;
        v.resize(nBams, std::vector<int>(nRegions));
        for (int r=0; r < nBams; ++r) {
            for (int c=0; c < nRegions; ++c) {
                if (allRows && c==iCol) {
                    v[r][c] = 1;
                } else if (allColumns && r==iRow) {
                    v[r][c] = 1;
                } else if (c==iCol && r==iRow) {
                    v[r][c] = 1;
                }
            }
        }
        return 1;
    }

    int Parser::split_into_or(std::string &s, std::vector<Eval> &evaluations, int nBams, int nRegions) {
        std::string delim;
        if (s.find("and") != std::string::npos) {
            delim = "and";
            orBlock = false;
        } else {
            delim = "or";
            orBlock = true;
        }

        int res = parse_indexing(s, nBams, nRegions, targetIndexes);
        if (res < 0) {
            return res;
        }

        auto start = 0U;
        auto end = s.find(delim);
        std::string token;

        // list of 'or' evaluations
        std::vector<std::vector<std::string> > allTokens;

        if (end == std::string::npos) {
            token = std::regex_replace(s, std::regex("^ +| +$|( ) +"), "$1");
            auto output = Utils::split(token, ' ');
            if (output.size() != 3) {
                std::cerr << "Expression not understood, length 3 required: " << token << std::endl;
                return -1;
            }
            allTokens.push_back(output);

        } else {
            while (end != std::string::npos)
            {
                token = s.substr(start, end - start);
                token = std::regex_replace(token, std::regex("^ +| +$|( ) +"), "$1");
                auto output = Utils::split(token, ' ');
                if (output.size() != 3) {
                    std::cerr << "Expression not understood, length 3 required: " << token << std::endl;
                    return -1;
                }
                allTokens.push_back(output);
                start = end + delim.length();
                end = s.find(delim, start);
            }
            token = s.substr(start, end - start);
            token = std::regex_replace(token, std::regex("^ +| +$|( ) +"), "$1");
            auto output = Utils::split(token, ' ');
            if (output.size() != 3) {
                std::cerr << "Expression not understood, length 3 required: " << token << std::endl;
                return -1;
            }
            allTokens.push_back(output);

        }
        for (auto &output: allTokens) {
            int res = prep_evaluations(evaluations, output);
            if (res < 0) {
                return res;
            }
        }
        return 1;
    }

    int Parser::prep_evaluations(std::vector<Eval> &evaluations, std::vector<std::string> &output) {
        if (! opMap.contains(output[0])) {
            std::cerr << "Left-hand side property not available: " << output[0] << std::endl;
            return -1;
        }
        if (! opMap.contains(output[1])) {
            std::cerr << "Middle operation not available: " << output[1] << std::endl;
            return -1;
        }
        Property lhs = opMap[output[0]];
        Property mid = opMap[output[1]];

        std::string allowed = permit[lhs];
        if (allowed.find(output[1]) == std::string::npos) {
            std::cerr << output[0] << " is only compatible with: " << allowed << std::endl;
            return -1;
        }

        Eval e;
        if (lhs >= 3000 && lhs < 4000) {
            e.property = lhs;
            e.op = mid;
            try {
                e.ival = std::stoi(output.back());
            } catch (...) {
                std::cerr << "Right-hand side operation not an integer: " << output[2] << std::endl;
                return -1;
            }
        } else if (lhs >= 4000) {
            e.property = lhs;
            e.op = mid;
            e.sval = output.back();
        } else {
            std::cerr << "Left-hand side operation not available: " << output[0] << std::endl; return -1;
        }

//        if (lhs == MAPQ || lhs == SEQ_LEN || lhs == TLEN || lhs == ABS_TLEN) {
//            e.property = lhs;
//            e.op = mid;
//            try {
//                e.ival = std::stoi(output.back());
//            } catch (...) {
//                std::cerr << "Right-hand side operation not an integer: " << output[2] << std::endl;
//                return -1;
//            }
//        } else if (lhs == FLAG || lhs == NFLAG) {
//            e.property = lhs;
//            e.op = mid;
//            try {
//                e.ival = std::stoi(output.back());
//            } catch (...) {
//                if (opMap.contains(output.back())) {
//                    e.ival = opMap[output.back()];
//                } else {
//                    std::cerr << "Right-hand side operation not understood: " << output[2] << std::endl;
//                    return -1;
//                }
//            }
//        } else if (lhs == SEQ || lhs == QNAME || lhs == RNEXT) {
//            e.property = lhs;
//            e.op = mid;
//            e.sval = output.back();
//        } else {
//            std::cerr << "Left-hand side operation not available: " << output[0] << std::endl; return -1;
//        }

        evaluations.push_back(e);
        return 1;
    }

    int Parser::set_filter(std::string &s, int nBams, int nRegions) {
        filter_str = s;
        if ( (s.find("or") != std::string::npos) && (s.find("and") != std::string::npos) ) {
            std::cerr << "Filter block must be either composed of 'or' expressions, or 'and' expressions, not both\n";
            return -1;
        }
        int res1 = split_into_or(s, evaluations_block, nBams, nRegions);
        return res1;
    }

    bool seq_contains(const uint8_t *seq, uint32_t len, const std::string &fstr) {
        auto slen = (int)fstr.size();
        int j;
        for (int i=0; i< (int)len ; i++){
            for (j=0 ; j < slen; j++) {
                if (fstr[j] != seq_nt16_str[bam_seqi(seq, i + j)]) {
                    break;
                }
            }
            if (j == slen - 1) {
                return true;
            }
        }
        return false;
    }

    bool seq_eq(const uint8_t *seq, uint32_t len, const std::string &fstr) {
        if (len != fstr.size()) {
            return false;
        }
        for (int i=0; i< (int)len ; i++){
            if ((char)fstr[i] != (char)seq_nt16_str[bam_seqi(seq, i)]) {
                return false;
            }
        }
        return true;
    }

    void process_ival(bool &this_result, Eval &e, int int_val) {
        switch (e.op) {
            case GT: this_result = int_val > e.ival; break;
            case GE: this_result = int_val >= e.ival; break;
            case LT: this_result = int_val < e.ival; break;
            case LE: this_result = int_val <= e.ival; break;
            case EQ: this_result = int_val == e.ival; break;
            case NE: this_result = int_val != e.ival; break;
            case AND: this_result = int_val & e.ival; break;
            default: break;
        }
    }

    void getStrTag(const char* tag, std::string &str_val, const Segs::Align &aln) {
        const uint8_t *tag_ptr;
        tag_ptr = bam_aux_get(aln.delegate, tag);
        if (tag_ptr == nullptr) {
            return;
        }
        str_val = std::string(bam_aux2Z(tag_ptr));
    }

    void getIntTag(const char* tag, int &int_val, const Segs::Align &aln) {
        const uint8_t *tag_ptr;
        tag_ptr = bam_aux_get(aln.delegate, tag);
        if (tag_ptr == nullptr) {
            return;
        }
        int_val = bam_aux2i(tag_ptr);
    }

    bool Parser::eval(const Segs::Align &aln, const sam_hdr_t* hdr, int bamIdx, int regionIdx) {

        bool block_result = true;

        if (!targetIndexes.empty() && targetIndexes[bamIdx][regionIdx] == 0) {
            return true;
        }

        if (!evaluations_block.empty()) {
            for (auto &e : evaluations_block) {
                int int_val = 0;
                std::string str_val;
                bool this_result = false;
                const char *char_ptr;
                switch (e.property) {
                    case MAPQ:
                        int_val = aln.delegate->core.qual;
                        break;
                    case SEQ_LEN:
                        int_val = aln.delegate->core.l_qseq;
                        break;
                    case FLAG:
                        int_val = aln.delegate->core.flag;
                        break;
                    case TLEN:
                        int_val = aln.delegate->core.isize;
                        break;
                    case ABS_TLEN:
                        int_val = std::abs(aln.delegate->core.isize);
                        break;
                    case NFLAG:
                        int_val = ~aln.delegate->core.flag;
                        break;
                    case QNAME:
                        str_val = bam_get_qname(aln.delegate);
                        break;
                    case POS:
                        int_val = aln.delegate->core.pos;
                        break;
                    case REF_END:
                        int_val = bam_endpos(aln.delegate);
                    case PNEXT:
                        int_val = aln.delegate->core.mpos;
                        break;
                    case RNEXT:
                        char_ptr = sam_hdr_tid2name(hdr, aln.delegate->core.mtid);
                        str_val = char_ptr;
                        break;
                    case RG:
                        getStrTag("RG", str_val, aln);
                        break;
                    case BC:
                        getStrTag("BC", str_val, aln);
                        break;
                    case LB:
                        getStrTag("LB", str_val, aln);
                        break;
                    case MD:
                        getStrTag("MD", str_val, aln);
                        break;
                    case PU:
                        getStrTag("PU", str_val, aln);
                        break;
                    case SA:
                        getStrTag("SA", str_val, aln);
                        break;
                    case MC:
                        getStrTag("MC", str_val, aln);
                        break;
                    case BX:
                        getStrTag("BX", str_val, aln);
                        break;
                    case RX:
                        getStrTag("RX", str_val, aln);
                        break;
                    case MI:
                        getStrTag("MI", str_val, aln);
                        break;
                    case NM:
                        getIntTag("NM", int_val, aln);
                        break;
                    case CM:
                        getIntTag("CM", int_val, aln);
                        break;
                    case FI:
                        getIntTag("FI", int_val, aln);
                        break;
                    case HO:
                        getIntTag("HO", int_val, aln);
                        break;
                    case MQ:
                        getIntTag("MQ", int_val, aln);
                        break;
                    case SM:
                        getIntTag("SM", int_val, aln);
                        break;
                    case TC:
                        getIntTag("TC", int_val, aln);
                        break;
                    case UQ:
                        getIntTag("UQ", int_val, aln);
                        break;
                    case AS:
                        getIntTag("AS", int_val, aln);
                        break;
                    default:
                        break;

                }
                if (e.property == SEQ) {
                    switch (e.op) {
                        case EQ: this_result = str_val == e.sval; break;
                        case NE: this_result = str_val != e.sval; break;
                        case CONTAINS: this_result = seq_contains(bam_get_seq(aln.delegate), aln.delegate->core.l_qseq, e.sval); break;
                        case OMIT: this_result = !seq_contains(bam_get_seq(aln.delegate), aln.delegate->core.l_qseq, e.sval); break;
                        default: break;
                    }

                } else if (str_val.empty()) {
                    switch (e.op) {
                        case GT: this_result = int_val > e.ival; break;
                        case GE: this_result = int_val >= e.ival; break;
                        case LT: this_result = int_val < e.ival; break;
                        case LE: this_result = int_val <= e.ival; break;
                        case EQ: this_result = int_val == e.ival; break;
                        case NE: this_result = int_val != e.ival; break;
                        case AND: this_result = int_val & e.ival; break;
                        default: break;
                    }
                } else {
                    switch (e.op) {
                        case EQ: this_result = str_val == e.sval; break;
                        case NE: this_result = str_val != e.sval; break;
                        case CONTAINS: this_result = str_val.find(e.sval) != std::string::npos; break;
                        case OMIT: this_result = str_val.find(e.sval) == std::string::npos; break;
                        default: break;
                    }
                }

                if (orBlock && this_result) {
                    return this_result;
                } else {
                    block_result &= this_result;
                }
            }
        }
        return block_result;
    }


    void countExpression(std::vector<Segs::ReadCollection> &collections, std::string &str, std::vector<sam_hdr_t*> hdrs,
                         std::vector<std::string> &bam_paths, int nBams, int nRegions) {

        std::vector<Parser> filters;
        for (auto &s: Utils::split(str, ';')) {
            Parse::Parser p = Parse::Parser();
            int rr = p.set_filter(s, nBams, nRegions);
            if (rr > 0) {
                filters.push_back(p);
            }
        }
        for (auto &col: collections) {
            int tot = 0;
            int paired = 0;
            int proper_pair = 0;
            int read_unmapped = 0;
            int mate_unmapped = 0;
            int read_reverse = 0;
            int mate_reverse = 0;
            int first = 0;
            int second = 0;
            int not_primary = 0;
            int fails_qc = 0;
            int duplicate = 0;
            int supp = 0;
            int del = 0;
            int dup = 0;
            int inv_f = 0;
            int inv_r = 0;
            int tra = 0;
            for (auto &align: col.readQueue) {
                bool drop = false;
                sam_hdr_t* hdr = hdrs[col.bamIdx];
                for (auto &f : filters) {
                    if (!f.eval(align, hdr, col.bamIdx, col.regionIdx)) {
                        drop = true;
                        break;
                    }
                }
                if (drop) {
                    continue;
                }
                uint32_t flag = align.delegate->core.flag;
                tot += 1;
                paired += bool(flag & 1);
                proper_pair += bool(flag & 2);
                read_unmapped += bool(flag & 4);
                mate_unmapped += bool(flag & 8);
                read_reverse += bool(flag & 16);
                mate_reverse += bool(flag & 32);
                first += bool(flag & 64);
                second += bool(flag & 128);
                not_primary += bool(flag & 256);
                fails_qc += bool(flag & 512);
                duplicate += bool(flag & 1024);
                supp += bool(flag & 2048);
                if (align.orient_pattern == Segs::DEL) {
                    del += 1;
                } else if (align.orient_pattern == Segs::DUP) {
                    dup += 1;
                } else if (align.orient_pattern == Segs::TRA) {
                    tra += 1;
                } else if (align.orient_pattern == Segs::INV_F) {
                    inv_f += 1;
                } else if (align.orient_pattern == Segs::INV_R) {
                    inv_r += 1;
                }
            }
            std::cout << termcolor::bright_blue << "File\t" << bam_paths[col.bamIdx] << termcolor::reset << std::endl;
            std::cout << "Region\t" << col.region.chrom << ":" << col.region.start << "-" << col.region.end << std::endl;
            if (!str.empty()) {
                std::cout << "Filter\t" << str << std::endl;
            }
            std::cout << "Total\t" << tot << std::endl;
            std::cout << "Paired\t" << paired << std::endl;
            std::cout << "Proper-pair\t" << proper_pair << std::endl;
            std::cout << "Read-unmapped\t" << read_unmapped << std::endl;
            std::cout << "Mate-unmapped\t" << mate_unmapped << std::endl;
            std::cout << "Read-reverse\t" << read_reverse << std::endl;
            std::cout << "Mate-reverse\t" << mate_reverse << std::endl;
            std::cout << "First-in-pair\t" << first << std::endl;
            std::cout << "Second-in-pair\t" << second << std::endl;
            std::cout << "Not-primary\t" << not_primary << std::endl;
            std::cout << "Fails-qc\t" << fails_qc << std::endl;
            std::cout << "Duplicate\t" << duplicate << std::endl;
            std::cout << "Supplementary\t" << supp << std::endl;
            if (del > 0)
                std::cout << "Deletion-pattern\t" << del << std::endl;
            if (dup > 0)
                std::cout << "Duplication-pattern\t" << dup << std::endl;
            if (tra > 0)
                std::cout << "Translocation-pattern\t" << tra << std::endl;
            if (inv_f > 0)
                std::cout << "F-inversion-pattern\t" << inv_f << std::endl;
            if (inv_r > 0)
                std::cout << "R-inversion-pattern\t" << inv_r << std::endl;
        }
    }
}