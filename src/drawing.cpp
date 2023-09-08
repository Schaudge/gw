//
// Created by Kez Cleal on 12/08/2022.
//
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <utility>
#include <stdio.h>
#include <GLFW/glfw3.h>

#define SK_GL
#include "include/core/SkCanvas.h"
#include "include/core/SkData.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkTypeface.h"
#include "include/core/SkTextBlob.h"

#include "htslib/sam.h"
#include "../include/BS_thread_pool.h"
#include "../include/unordered_dense.h"
#include "hts_funcs.h"
#include "drawing.h"



namespace Drawing {

    char indelChars[50];
    constexpr float polygonHeight = 0.85;

    struct Mismatches {
        uint32_t A, T, C, G;
    };

    void drawCoverage(const Themes::IniOptions &opts, std::vector<Segs::ReadCollection> &collections,
                      SkCanvas *canvas, const Themes::Fonts &fonts, const float covYh, const float refSpace) {

        const Themes::BaseTheme &theme = opts.theme;
        SkPaint paint = theme.fcCoverage;
        SkPath path;
        std::vector<sk_sp < SkTextBlob> > text;
        std::vector<sk_sp < SkTextBlob> > text_ins;
        std::vector<float> textX, textY;
        std::vector<float> textX_ins, textY_ins;

        float covY = covYh * 0.95;

        int last_bamIdx = 0;
        float yOffsetAll = refSpace;

        for (auto &cl: collections) {
            if (cl.covArr.empty() || cl.readQueue.empty()) {
                continue;
            }
            if (cl.bamIdx != last_bamIdx) {
                yOffsetAll += cl.yPixels;
            }
            float xScaling = cl.xScaling;
            float xOffset = cl.xOffset;
            float tot, mean, n;
            const std::vector<int> & covArr_r = cl.covArr;
            std::vector<float> c;
            c.resize(cl.covArr.size());
            c[0] = (float)cl.covArr[0];
            int cMaxi = (c[0] > 10) ? (int)c[0] : 10;
            tot = (float)c[0];
            n = 0;
            if (tot > 0) {
                n += 1;
            }
            float cMax;
            for (size_t i=1; i<c.size(); ++i) { // cum sum
                c[i] = ((float)covArr_r[i]) + c[i-1];
                if (c[i] > cMaxi) {
                    cMaxi = (int)c[i];
                }
                if (c[i] > 0) {
                    tot += c[i];
                    n += 1;
                }
            }
            cl.maxCoverage = cMaxi;
            if (n > 0) {
                mean = tot / n;
                mean = ((float)((int)(mean * 10))) / 10;
            } else {
                mean = 0;
            }

            if (opts.log2_cov) {
                for (size_t i=0; i<c.size(); ++i) {
                    if (c[i] > 0) { c[i] = std::log2(c[i]); }
                }
                cMax = std::log2(cMaxi);
            } else if (cMaxi < opts.max_coverage) {
                cMax = cMaxi;
            } else {
                cMax = (float)opts.max_coverage;
                cMaxi = (int)cMax;
            }
            // normalize to space available
            for (auto &i : c) {
                if (i > cMax) {
                    i = 0;
                } else {
                    i = ((1 - (i / cMax)) * covY) * 0.7;
                }
                i += yOffsetAll + (covY * 0.3);
            }
            int step;
            if (c.size() > 2000) {
                step = std::max(1, (int)(c.size() / 2000));
            } else {
                step = 1;
            }

            float lastY = yOffsetAll + covY;
            double x = xOffset;

            path.reset();
            path.moveTo(x, lastY);
            for (size_t i=0; i<c.size(); ++i)  {
                if (i % step == 0 || i == c.size() - 1) {
                    path.lineTo(x, lastY);
                    path.lineTo(x, c[i]);
                }
                lastY = c[i];
                x += xScaling;
            }
            path.lineTo(x - xScaling, yOffsetAll + covY);
            path.lineTo(xOffset, yOffsetAll + covY);
            path.close();
            canvas->drawPath(path, paint);

            std::sprintf(indelChars, "%d", cMaxi);

            sk_sp<SkTextBlob> blob = SkTextBlob::MakeFromString(indelChars, fonts.overlay);
            canvas->drawTextBlob(blob, xOffset + 25, (covY * 0.3) + yOffsetAll + 10, theme.tcDel);
            path.reset();
            path.moveTo(xOffset, (covY * 0.3) + yOffsetAll);
            path.lineTo(xOffset + 20, (covY * 0.3) + yOffsetAll);
            path.moveTo(xOffset, covY + yOffsetAll);
            path.lineTo(xOffset + 20, covY + yOffsetAll);
            canvas->drawPath(path, theme.lcJoins);

            char * ap = indelChars;
            ap += std::sprintf(indelChars, "%s", "avg. ");
            std::sprintf(ap, "%.1f", mean);

            if (((covY * 0.5) + yOffsetAll + 10 - fonts.overlayHeight) - ((covY * 0.3) + yOffsetAll + 10) > 0) { // dont overlap text
                blob = SkTextBlob::MakeFromString(indelChars, fonts.overlay);
                canvas->drawTextBlob(blob, xOffset + 25, (covY * 0.5) + yOffsetAll + 10, theme.tcDel);
            }
            last_bamIdx = cl.bamIdx;
        }
    }

    inline void chooseFacecolors(int mapq, const Segs::Align &a, SkPaint &faceColor, const Themes::BaseTheme &theme) {
        if (mapq == 0) {
            switch (a.orient_pattern) {
                case Segs::NORMAL:
                    faceColor = theme.fcNormal0;
                    break;
                case Segs::DEL:
                    faceColor = theme.fcDel0;
                    break;
                case Segs::INV_F:
                    faceColor = theme.fcInvF0;
                    break;
                case Segs::INV_R:
                    faceColor = theme.fcInvR0;
                    break;
                case Segs::DUP:
                    faceColor = theme.fcDup0;
                    break;
                case Segs::TRA:
                    faceColor = theme.mate_fc0[a.delegate->core.mtid % 48];
                    break;
            }
        } else {
            switch (a.orient_pattern) {
                case Segs::NORMAL:
                    faceColor = theme.fcNormal;
                    break;
                case Segs::DEL:
                    faceColor = theme.fcDel;
                    break;
                case Segs::INV_F:
                    faceColor = theme.fcInvF;
                    break;
                case Segs::INV_R:
                    faceColor = theme.fcInvR;
                    break;
                case Segs::DUP:
                    faceColor = theme.fcDup;
                    break;
                case Segs::TRA:
                    faceColor = theme.mate_fc[a.delegate->core.mtid % 48];
                    break;
            }
        }
    }

    inline void chooseEdgeColor(int edge_type, SkPaint &edgeColor, const Themes::BaseTheme &theme) {
        if (edge_type == 2) {
            edgeColor = theme.ecSplit;
        } else if (edge_type == 4) {
            edgeColor = theme.ecSelected;
        } else {
            edgeColor = theme.ecMateUnmapped;
        }
    }

    inline void
    drawRectangle(SkCanvas *canvas, const float polygonH, const float yScaledOffset, const float start, const float width, const float xScaling,
                  const float xOffset, const SkPaint &faceColor, SkRect &rect) {
        rect.setXYWH((start * xScaling) + xOffset, yScaledOffset, width * xScaling, polygonH);
        canvas->drawRect(rect, faceColor);
    }

    inline void
    drawLeftPointedRectangle(SkCanvas *canvas, const float polygonH, const float yScaledOffset, float start, float width,
                             const float xScaling, const float maxX, const float xOffset, const SkPaint &faceColor, SkPath &path, const float slop) {
        start *= xScaling;
        width *= xScaling;
        if (start < 0) {
            width += start;
            start = 0;
        }
        if (start + width > maxX) {
            width = maxX - start;
        }
        path.reset();
        path.moveTo(start + xOffset, yScaledOffset);
        path.lineTo(start - slop + xOffset, yScaledOffset + (polygonH * 0.5));
        path.lineTo(start + xOffset, yScaledOffset + polygonH);
        path.lineTo(start + width + xOffset, yScaledOffset + polygonH);
        path.lineTo(start + width + xOffset, yScaledOffset);
        path.close();
        canvas->drawPath(path, faceColor);
    }

    inline void
    drawRightPointedRectangle(SkCanvas *canvas, const float polygonH, const float yScaledOffset, float start, float width,
                              const float xScaling, const float maxX, const float xOffset, const SkPaint &faceColor, SkPath &path,
                              const float slop) {
        start *= xScaling;
        width *= xScaling;
        if (start < 0) {
            width += start;
            start = 0;
        }
        if (start + width > maxX) {
            width = maxX - start;
        }
        path.reset();
        path.moveTo(start + xOffset, yScaledOffset);
        path.lineTo(start + xOffset, yScaledOffset + polygonH);
        path.lineTo(start + width + xOffset, yScaledOffset + polygonH);
        path.lineTo(start + width + slop + xOffset, yScaledOffset + (polygonH * 0.5));
        path.lineTo(start + width + xOffset, yScaledOffset);
        path.close();
        canvas->drawPath(path, faceColor);
    }

    inline void drawHLine(SkCanvas *canvas, SkPath &path, const SkPaint &lc, const float startX, const float y, const float endX) {
        path.reset();
        path.moveTo(startX, y);
        path.lineTo(endX, y);
        canvas->drawPath(path, lc);
    }

    void drawIns(SkCanvas *canvas, float y0, float start, float yScaling, float xOffset,
                 float yOffset, float textW, const SkPaint &sidesColor, const SkPaint &faceColor, SkPath &path,
                 SkRect &rect) {

        float x = start + xOffset;
        float y = y0 * yScaling;
        float ph = polygonHeight * yScaling;
        float overhang = textW * 0.125;
        float text_half = textW * 0.5;
        rect.setXYWH(x - text_half - 2, y + yOffset, textW + 2, ph);
        canvas->drawRect(rect, faceColor);

        path.reset();
        path.moveTo(x - text_half - overhang, yOffset + y + ph * 0.05);
        path.lineTo(x + text_half + overhang, yOffset + y + ph * 0.05);
        path.moveTo(x - text_half - overhang, yOffset + y + ph * 0.95);
        path.lineTo(x + text_half + overhang, yOffset + y + ph * 0.95);
        path.moveTo(x, yOffset + y);
        path.lineTo(x, yOffset + y + ph);
        canvas->drawPath(path, sidesColor);
    }

    void drawMismatchesNoMD(SkCanvas *canvas, SkRect &rect, const Themes::BaseTheme &theme, const Utils::Region &region, const Segs::Align &align,
                            float width, float xScaling, float xOffset, float mmPosOffset, float yScaledOffset, float pH, int l_qseq, std::vector<Mismatches> &mm_array) {
        uint32_t r_pos = align.pos;
        uint32_t cigar_l = align.delegate->core.n_cigar; //align.cigar_l;
        uint8_t *ptr_seq = bam_get_seq(align.delegate);
        uint32_t *cigar_p = bam_get_cigar(align.delegate);
        auto *ptr_qual = bam_get_qual(align.delegate);
        int r_idx;
        uint32_t idx = 0;
        const char *refSeq = region.refSeq;
        if (refSeq == nullptr) {
            return;
        }
        uint32_t rlen = region.end - region.start;
        uint32_t rbegin = (uint32_t)region.start;
        uint32_t rend = (uint32_t)region.end;
        uint32_t op, l, colorIdx;
        float p;
        for (int k = 0; k < (int)cigar_l; k++) {
            op = cigar_p[k] & BAM_CIGAR_MASK;
            l = cigar_p[k] >> BAM_CIGAR_SHIFT;
            if (op == BAM_CSOFT_CLIP) {
                idx += l;
                continue;
            }
            else if (op == BAM_CINS) {
                idx += l;
                continue;
            }
            else if (op == BAM_CDEL) {
                r_pos += l;
                continue;
            }
            else if (op == BAM_CREF_SKIP) {
                r_pos += l;
                continue;
            }
            else if (op == BAM_CHARD_CLIP || op == BAM_CEQUAL) {
                continue;
            }
            else if (op == BAM_CDIFF) {
                for (int i=0; i < l; ++l) {
                    if (r_pos >= rbegin && r_pos < rend) {
                        char bam_base = bam_seqi(ptr_seq, idx);
                        p = (float)(r_pos - rbegin) * xScaling;
                        colorIdx = (l_qseq == 0) ? 10 : (ptr_qual[idx] > 10) ? 10 : ptr_qual[idx];
                        rect.setXYWH(p + xOffset + mmPosOffset, yScaledOffset, width, pH);
                        canvas->drawRect(rect, theme.BasePaints[bam_base][colorIdx]);
                        switch (bam_base) {
                            case 1: mm_array[r_pos - rbegin].A += 1; break;  // A==1, C==2, G==4, T==8, N==>8
                            case 2: mm_array[r_pos - rbegin].C += 1; break;
                            case 4: mm_array[r_pos - rbegin].G += 1; break;
                            case 8: mm_array[r_pos - rbegin].T += 1; break;
                            default: break;
                        }
                    }
                    idx += 1;
                    r_pos += 1;
                }
            }
            else {  // BAM_CMATCH
                for (int i=0; i < l; ++i) {
                    r_idx = (int)r_pos - region.start;
                    if (r_idx < 0) {
                        idx += 1;
                        r_pos += 1;
                        continue;
                    }
                    if (r_idx >= rlen) {
                        break;
                    }
                    char ref_base;
                    switch (refSeq[r_idx]) {
                        case 'A': ref_base = 1; break;
                        case 'C': ref_base = 2; break;
                        case 'G': ref_base = 4; break;
                        case 'T': ref_base = 8; break;
                        case 'N': ref_base = 15; break;
                        case 'a': ref_base = 1; break;
                        case 'c': ref_base = 2; break;
                        case 'g': ref_base = 4; break;
                        case 't': ref_base = 8; break;
                        default: ref_base = 15; break;
                    }
                    char bam_base = bam_seqi(ptr_seq, idx);
                    if (bam_base != ref_base) {
                        p = (float)(r_pos - rbegin) * xScaling;
                        colorIdx = (l_qseq == 0) ? 10 : (ptr_qual[idx] > 10) ? 10 : ptr_qual[idx];
                        rect.setXYWH(p + xOffset + mmPosOffset, yScaledOffset, width, pH);
                        canvas->drawRect(rect, theme.BasePaints[bam_base][colorIdx]);
                        switch (bam_base) {
                            case 1: mm_array[r_pos - rbegin].A += 1; break;  // A==1, C==2, G==4, T==8, N==>8
                            case 2: mm_array[r_pos - rbegin].C += 1; break;
                            case 4: mm_array[r_pos - rbegin].G += 1; break;
                            case 8: mm_array[r_pos - rbegin].T += 1; break;
                            default: break;
                        }
                    }
                    idx += 1;
                    r_pos += 1;
                }
            }
        }
    }

    void drawBlock(bool plotPointedPolygons, bool pointLeft, bool edged, float s, float e, float width,
                   float pointSlop, float pH, float yScaledOffset, float xScaling, float xOffset, float regionPixels,
                   size_t idx, size_t nBlocks, int regionLen,
                   const Segs::Align& a, SkCanvas *canvas, SkPath &path, SkRect &rect, SkPaint &faceColor, SkPaint &edgeColor) {

        if (plotPointedPolygons) {
            if (pointLeft) {
                if (s > 0 && idx == 0 && a.left_soft_clip == 0) {
                    drawLeftPointedRectangle(canvas, pH, yScaledOffset, s, width, xScaling,
                                             regionPixels, xOffset, faceColor, path, pointSlop);
                    if (edged) {
                        drawLeftPointedRectangle(canvas, pH, yScaledOffset, s, width, xScaling,
                                                 regionPixels, xOffset, edgeColor, path, pointSlop);
                    }
                } else {
                    drawRectangle(canvas, pH, yScaledOffset, s, width, xScaling, xOffset,
                                  faceColor, rect);
                    if (edged) {
                        drawRectangle(canvas, pH, yScaledOffset, s, width, xScaling, xOffset,
                                      edgeColor, rect);
                    }
                }
            } else {
                if (e < regionLen && idx == nBlocks - 1 && a.right_soft_clip == 0) {
                    drawRightPointedRectangle(canvas, pH, yScaledOffset, s, width, xScaling,
                                              regionPixels, xOffset, faceColor, path, pointSlop);
                    if (edged) {
                        drawRightPointedRectangle(canvas, pH, yScaledOffset, s, width, xScaling,
                                                  regionPixels, xOffset, edgeColor, path, pointSlop);
                    }
                } else {
                    drawRectangle(canvas, pH, yScaledOffset, s, width, xScaling, xOffset,
                                  faceColor, rect);
                    if (edged) {
                        drawRectangle(canvas, pH, yScaledOffset, s, width, xScaling, xOffset,
                                      edgeColor, rect);
                    }
                }
            }
        } else {
            drawRectangle(canvas, pH, yScaledOffset, s, width, xScaling, xOffset, faceColor,
                          rect);
            if (edged) {
                drawRectangle(canvas, pH, yScaledOffset, s, width, xScaling, xOffset,
                              edgeColor, rect);
            }
        }
    }

    void drawDeletionLine(const Segs::Align& a, SkCanvas *canvas, SkPath &path, const Themes::IniOptions &opts, const Themes::Fonts &fonts,
                          int regionBegin, size_t idx, int Y, int regionLen, int starti, int lastEndi,
                          float regionPixels, float xScaling, float yScaling, float xOffset, float yOffset, float textDrop,
                          std::vector<sk_sp <SkTextBlob> > &text, std::vector<float> &textX, std::vector<float> &textY) {

        int isize = starti - lastEndi;
        int lastEnd = lastEndi - regionBegin;
        starti -= regionBegin;

        lastEnd = (lastEnd < 0) ? 0 : lastEnd;
        int size = starti - lastEnd;
        if (size <= 0) {
            return;
        }
        float delBegin = (float)lastEnd * xScaling;
        float delEnd = delBegin + ((float)size * xScaling);
        float yh = ((float)Y + (float)polygonHeight * (float)0.5) * yScaling + yOffset;

        if (isize >= opts.indel_length) {
            if (regionLen < 500000) { // line and text
                std::sprintf(indelChars, "%d", isize);
                size_t sl = strlen(indelChars);
                float textW = fonts.textWidths[sl - 1];
                float textBegin = (((float)lastEnd + (float)size / 2) * xScaling) - (textW / 2);
                float textEnd = textBegin + textW;
                if (textBegin < 0) {
                    textBegin = 0;
                    textEnd = textW;
                } else if (textEnd > regionPixels) {
                    textBegin = regionPixels - textW;
                    textEnd = regionPixels;
                }
                text.push_back(SkTextBlob::MakeFromString(indelChars, fonts.fonty));
                textX.push_back(textBegin + xOffset);
                textY.push_back(((float)Y + polygonHeight) * yScaling - textDrop + yOffset);
                if (textBegin > delBegin) {
                    drawHLine(canvas, path, opts.theme.lcJoins, delBegin + xOffset, yh, textBegin + xOffset);
                    drawHLine(canvas, path, opts.theme.lcJoins, textEnd + xOffset, yh, delEnd + xOffset);
                }
            } else { // dot only
                delEnd = std::min(regionPixels, delEnd);
                if (delEnd - delBegin < 2) {
                    canvas->drawPoint(delBegin + xOffset, yh, opts.theme.lcBright);
                } else {
                    drawHLine(canvas, path, opts.theme.lcJoins, delBegin + xOffset, yh, delEnd + xOffset);
                }
            }
        } else if ((float)size / (float) regionLen > 0.0005) { // (regionLen < 50000 || size > 100) { // line only
            delEnd = std::min(regionPixels, delEnd);
            drawHLine(canvas, path, opts.theme.lcJoins, delBegin + xOffset, yh, delEnd + xOffset);
        }
    }

    void drawBams(const Themes::IniOptions &opts, const std::vector<Segs::ReadCollection> &collections,
                  SkCanvas *canvas, float trackY, float yScaling, const Themes::Fonts &fonts, int linkOp, float refSpace) {

        SkPaint faceColor;
        SkPaint edgeColor;

        SkRect rect;
        SkPath path;

        const Themes::BaseTheme &theme = opts.theme;

        std::vector<sk_sp < SkTextBlob> > text;
        std::vector<sk_sp < SkTextBlob> > text_ins;
        std::vector<float> textX, textY;
        std::vector<float> textX_ins, textY_ins;

        for (auto &cl: collections) {

            int regionBegin = cl.region.start;
            int regionEnd = cl.region.end;
            int regionLen = regionEnd - regionBegin;

            float xScaling = cl.xScaling;
            float xOffset = cl.xOffset;
            float yOffset = cl.yOffset;
            float regionPixels = regionLen * xScaling;
            float pointSlop = (tan(0.42) * (yScaling * 0.5));  // radians
            float textDrop = (yScaling - fonts.fontHeight) * 0.5;

            bool plotSoftClipAsBlock = regionLen > opts.soft_clip_threshold;
            bool plotPointedPolygons = regionLen < 50000;
            bool drawEdges = regionLen < opts.edge_highlights;

            float pH = yScaling * polygonHeight;
            if (opts.tlen_yscale) {
                pH = trackY / (float)opts.ylim;
            }
            if (pH > 10) {  // scale to pixel boundary
                pH = (float)(int)pH;
            }

            std::vector<Mismatches> mm_vector;
            if (opts.max_coverage > 0) {
                mm_vector.resize(regionEnd - regionBegin);
            }

            for (auto &a: cl.readQueue) {

                int Y = a.y;
                if (Y == -1) {
                    continue;
                }
                int mapq = a.delegate->core.qual;
                float yScaledOffset = (Y * yScaling) + yOffset;
                chooseFacecolors(mapq, a, faceColor, theme);
                bool pointLeft, edged;
                if (plotPointedPolygons) {
                    pointLeft = (a.delegate->core.flag & 16) != 0;
                } else {
                    pointLeft = false;
                }
                size_t nBlocks = a.block_starts.size();
                if (drawEdges && a.edge_type != 1) {
                    edged = true;
                    chooseEdgeColor(a.edge_type, edgeColor, theme);
                } else {
                    edged = false;
                }
                double width, s, e, textW;
                int lastEnd = 1215752191;
                int starti;
                bool line_only;
                for (size_t idx = 0; idx < nBlocks; ++idx) {
                    starti = (int)a.block_starts[idx];
                    if (idx > 0) {
                        lastEnd = (int)a.block_ends[idx-1];
                    }

                    if (starti > regionEnd) {
                        if (lastEnd < regionEnd) {
                            line_only = true;
                        } else {
                            break;
                        }
                    } else {
                        line_only = false;
                    }

                    e = (double)a.block_ends[idx];
                    if (e < regionBegin) { continue; }
                    s = starti - regionBegin;
                    e -= regionBegin;
                    s = (s < 0) ? 0: s;
                    e = (e > regionLen) ? regionLen : e;
                    width = e - s;
                    if (!line_only) {
                        drawBlock(plotPointedPolygons, pointLeft, edged, (float)s, (float)e, (float)width,
                                pointSlop, pH, yScaledOffset, xScaling, xOffset, regionPixels,
                                idx, nBlocks, regionLen,
                                a, canvas, path, rect, faceColor, edgeColor);
                    }

                    // add lines and text between gaps
                    if (idx > 0) {
                        drawDeletionLine(a, canvas, path, opts, fonts,
                                regionBegin, idx, Y, regionLen, starti, lastEnd,
                                regionPixels, xScaling, yScaling, xOffset, yOffset, textDrop,
                                text, textX, textY);
                    }
                }
                // add soft-clip blocks
                int start = (int)a.pos - regionBegin;
                int end = (int)a.reference_end - regionBegin;
                auto l_seq = (int)a.delegate->core.l_qseq;
                if (opts.soft_clip_threshold != 0) {
                    if (a.left_soft_clip > 0) {
                        width = (plotSoftClipAsBlock || l_seq == 0) ? (float) a.left_soft_clip : 0;
                        s = start - a.left_soft_clip;
                        if (s < 0) {
                            width += s;
                            s = 0;
                        }
                        e = start + width;
                        if (start > regionLen) {
                            width = regionLen - start;
                        }
                        if (e > 0 && s < regionLen && width > 0) {
                            if (pointLeft && plotPointedPolygons) {
                                drawLeftPointedRectangle(canvas, pH, yScaledOffset, s, width, xScaling,
                                                         regionPixels, xOffset,
                                                         (mapq == 0) ? theme.fcSoftClip0 : theme.fcSoftClip,
                                                         path, pointSlop);
                            } else {
                                drawRectangle(canvas, pH, yScaledOffset, s, width, xScaling, xOffset,
                                              (mapq == 0) ? theme.fcSoftClip0 : theme.fcSoftClip, rect);
                            }
                        }
                    }
                    if (a.right_soft_clip > 0) {
                        if (plotSoftClipAsBlock || l_seq == 0) {
                            s = end;
                            width = (float) a.right_soft_clip;
                        } else {
                            s = end + a.right_soft_clip;
                            width = 0;
                        }
                        e = s + width;
                        if (s < 0) {
                            width += s;
                            s = 0;
                        }
                        if (e > regionLen) {
                            width = regionLen - s;
                            e = regionLen;
                        }
                        if (s < regionLen && e > 0) {
                            if (!pointLeft && plotPointedPolygons) {
                                drawRightPointedRectangle(canvas, pH, yScaledOffset, s, width, xScaling,
                                                          regionPixels, xOffset,
                                                          (mapq == 0) ? theme.fcSoftClip0 : theme.fcSoftClip, path,
                                                          pointSlop);
                            } else {
                                drawRectangle(canvas, pH, yScaledOffset, s, width, xScaling,
                                              xOffset, (mapq == 0) ? theme.fcSoftClip0 : theme.fcSoftClip, rect);
                            }
                        }
                    }
                }

                // add insertions
                if (!a.any_ins.empty()) {
                    for (auto &ins: a.any_ins) {
                        float p = (ins.pos - regionBegin) * xScaling;
                        if (0 <= p && p < regionPixels) {
                            std::sprintf(indelChars, "%d", ins.length);
                            size_t sl = strlen(indelChars);
                            textW = fonts.textWidths[sl - 1];
                            if (ins.length > (uint32_t)opts.indel_length) {
                                if (regionLen < 500000) {  // line and text
                                    drawIns(canvas, Y, p, yScaling, xOffset, yOffset, textW, theme.insS,
                                            theme.fcIns, path, rect);
                                    text_ins.push_back(SkTextBlob::MakeFromString(indelChars, fonts.fonty));
                                    textX_ins.push_back(p - (textW * 0.5) + xOffset - 2);
                                    textY_ins.push_back(((Y + polygonHeight) * yScaling) + yOffset - textDrop);
                                } else {  // line only
                                    drawIns(canvas, Y, p, yScaling, xOffset, yOffset, xScaling, theme.insS,
                                            theme.fcIns, path, rect);
                                }
                            } else if (regionLen < 100000 && regionLen < opts.small_indel_threshold) {  // line only
                                drawIns(canvas, Y, p, yScaling, xOffset, yOffset, xScaling, theme.insS,
                                        theme.fcIns, path, rect);
                            }
                        }
                    }
                }

                // add mismatches
                if (regionLen > opts.snp_threshold && plotSoftClipAsBlock) {
                    continue;
                }
                if (l_seq == 0) {
                    continue;
                }
                float mmPosOffset, mmScaling;
                if (regionLen < 500) {
                    mmPosOffset = 0.05;
                    mmScaling = 0.9;
                } else {
                    mmPosOffset = 0;
                    mmScaling = 1;
                }
                int colorIdx;

                int32_t l_qseq = a.delegate->core.l_qseq;

                if (regionLen <= opts.snp_threshold) {
                    float mms = xScaling * mmScaling;
                    width = (regionLen < 500000) ? ((1. > mms) ? 1. : mms) : xScaling;
                    drawMismatchesNoMD(canvas, rect, theme, cl.region, a, (float)width, xScaling, xOffset, mmPosOffset, yScaledOffset, pH, l_qseq, mm_vector);
                }

                // add soft-clips
                if (!plotSoftClipAsBlock) {
                    uint8_t *ptr_seq = bam_get_seq(a.delegate);
                    uint8_t *ptr_qual = bam_get_qual(a.delegate);
                    if (a.right_soft_clip > 0) {
                        int pos = (int)a.reference_end - regionBegin;
                        if (pos < regionLen && a.cov_end > regionBegin) {
                            int opLen = (int)a.right_soft_clip;
                            for (int idx = l_seq - opLen; idx < l_seq; ++idx) {
                                float p = pos * xScaling;
                                if (0 <= p && p < regionPixels) {
                                    uint8_t base = bam_seqi(ptr_seq, idx);
                                    uint8_t qual = ptr_qual[idx];
                                    colorIdx = (l_qseq == 0) ? 10 : (qual > 10) ? 10 : qual;
                                    rect.setXYWH(p + xOffset + mmPosOffset, yScaledOffset, xScaling * mmScaling, pH);
                                    canvas->drawRect(rect, theme.BasePaints[base][colorIdx]);
                                } else if (p > regionPixels) {
                                    break;
                                }
                                pos += 1;
                            }
                        }
                    }
                    if (a.left_soft_clip > 0) {
                        int opLen = (int)a.left_soft_clip;
                        int pos = (int)a.pos - regionBegin - opLen;
                        for (int idx = 0; idx < opLen; ++idx) {
                            float p = pos * xScaling;
                            if (0 <= p && p < regionPixels) {
                                uint8_t base = bam_seqi(ptr_seq, idx);
                                uint8_t qual = ptr_qual[idx];
                                colorIdx = (l_qseq == 0) ? 10 : (qual > 10) ? 10 : qual;
                                rect.setXYWH(p + xOffset + mmPosOffset, yScaledOffset, xScaling * mmScaling, pH);
                                canvas->drawRect(rect, theme.BasePaints[base][colorIdx]);
                            } else if (p >= regionPixels) {
                                break;
                            }
                            pos += 1;
                        }
                    }
                }
            }

            // draw mismatch blocks on the coverage track
            if (opts.max_coverage > 0) {
                int i = 0;
                for (const auto &item : mm_vector) {
                    uint32_t sum = item.A + item.T + item.C + item.G;
                    if (!sum) { i+=1; continue; }
//                    std::cerr << "sum " << sum << " maxC " << cl.maxCoverage << std::endl;

                    i += 1;
                }
            }

            // draw markers
            if (cl.region.markerPos != -1) {
                float rp = refSpace + 6 + (cl.bamIdx * cl.yPixels);
                float xp = refSpace * 0.3;
                float markerP = (xScaling * (float)(cl.region.markerPos - cl.region.start)) + cl.xOffset;
                if (markerP > cl.xOffset && markerP < regionPixels - cl.xOffset) {
                    path.reset();
                    path.moveTo(markerP, rp);
                    path.lineTo(markerP - xp, rp);
                    path.lineTo(markerP, rp + refSpace);
                    path.lineTo(markerP + xp, rp);
                    path.lineTo(markerP, rp);
                    canvas->drawPath(path, theme.marker_paint);
                }
                float markerP2 = (xScaling * (float)(cl.region.markerPosEnd - cl.region.start)) + cl.xOffset;
                if (markerP2 > cl.xOffset && markerP2 < (regionPixels + cl.xOffset)) {
                    path.reset();
                    path.moveTo(markerP2, rp);
                    path.lineTo(markerP2 - xp, rp);
                    path.lineTo(markerP2, rp + refSpace);
                    path.lineTo(markerP2 + xp, rp);
                    path.lineTo(markerP2, rp);
                    canvas->drawPath(path, theme.marker_paint);
                }
            }

            // draw text last
            for (int i = 0; i < (int)text.size(); ++i) {
                canvas->drawTextBlob(text[i].get(), textX[i], textY[i], theme.tcDel);
            }
            for (int i = 0; i < (int)text_ins.size(); ++i) {
                canvas->drawTextBlob(text_ins[i].get(), textX_ins[i], textY_ins[i], theme.tcIns);
            }
        }

        // draw connecting lines between linked alignments
        if (linkOp > 0) {
            for (auto const &rc: collections) {

                if (!rc.linked.empty()) {
                    const Segs::map_t &lm = rc.linked;
                    SkPaint paint;
                    for (auto const& keyVal : lm) {
                        const std::vector<Segs::Align *> &ind = keyVal.second;
                        int size = (int)ind.size();
                        if (size > 1) {
                            float max_x = rc.xOffset + (((float)rc.region.end - (float)rc.region.start) * rc.xScaling);
                            for (int jdx=0; jdx < size - 1; ++jdx) {

                                const Segs::Align *segA = ind[jdx];
                                const Segs::Align *segB = ind[jdx + 1];

                                if (segA->y == -1 || segB->y == -1 || segA->block_ends.empty() || segB->block_ends.empty() || (segA->delegate->core.tid != segB->delegate->core.tid)) { continue; }

                                long cstart = std::min(segA->block_ends.front(), segB->block_ends.front());
                                long cend = std::max(segA->block_starts.back(), segB->block_starts.back());
                                double x_a = ((double)cstart - (double)rc.region.start) * rc.xScaling;
                                double x_b = ((double)cend - (double)rc.region.start) * rc.xScaling;

                                x_a = (x_a < 0) ? 0: x_a;
                                x_b = (x_b < 0) ? 0 : x_b;
                                x_a += rc.xOffset;
                                x_b += rc.xOffset;
                                x_a = (x_a > max_x) ? max_x : x_a;
                                x_b = (x_b > max_x) ? max_x : x_b;
                                float y = ((float)segA->y * yScaling) + ((polygonHeight / 2) * yScaling) + rc.yOffset;

                                switch (segA->orient_pattern) {
                                    case Segs::DEL: paint = theme.fcDel; break;
                                    case Segs::DUP: paint = theme.fcDup; break;
                                    case Segs::INV_F: paint = theme.fcInvF; break;
                                    case Segs::INV_R: paint = theme.fcInvR; break;
                                    default: paint = theme.fcNormal; break;
                                }
                                paint.setStyle(SkPaint::kStroke_Style);
                                paint.setStrokeWidth(2);
                                path.reset();
                                path.moveTo(x_a, y);
                                path.lineTo(x_b, y);
                                canvas->drawPath(path, paint);
                            }
                        }
                    }
                }
            }
        }
    }

    void drawRef(const Themes::IniOptions &opts,
                  std::vector<Utils::Region> &regions, int fb_width,
                  SkCanvas *canvas, const Themes::Fonts &fonts, float h, float nRegions, float gap) {
        if (regions.empty()) {
            return;
        }
        SkRect rect;
        SkPaint faceColor;
        const Themes::BaseTheme &theme = opts.theme;
        double regionW = (double)fb_width / (double)regions.size();
        double xPixels = regionW - gap - gap;
        float textW = fonts.overlayWidth;
        float minLetterSize;
        minLetterSize = (textW > 0) ? ((float)fb_width / (float)regions.size()) / textW : 0;
        int index = 0;
        h *= 0.7;
        for (auto &rgn: regions) {
            int size = rgn.end - rgn.start;
            double xScaling = xPixels / size;
            const char *ref = rgn.refSeq;
            if (ref == nullptr) {
                continue;
            }
            double mmPosOffset, mmScaling;
            if (size < 250) {
                mmPosOffset = 0.05;
                mmScaling = 0.9;
            } else {
                mmPosOffset = h * 0.2;
                mmScaling = 1;
            }
            double i = regionW * index;
            i += gap;
            if (textW > 0 && (float)size < minLetterSize && fonts.fontHeight < h) {
                double v = (xScaling - textW) * 0.5;
                float yp = h;
                while (*ref) {
                    switch ((unsigned int)*ref) {
                        case 65: faceColor = theme.fcA; break;
                        case 67: faceColor = theme.fcC; break;
                        case 71: faceColor = theme.fcG; break;
                        case 78: faceColor = theme.fcN; break;
                        case 84: faceColor = theme.fcT; break;
                        case 97: faceColor = theme.fcA; break;
                        case 99: faceColor = theme.fcC; break;
                        case 103: faceColor = theme.fcG; break;
                        case 110: faceColor = theme.fcN; break;
                        case 116: faceColor = theme.fcT; break;
                    }
                    canvas->drawTextBlob(SkTextBlob::MakeFromText(ref, 1, fonts.overlay, SkTextEncoding::kUTF8),
                                         i + v, yp, faceColor);
                    i += xScaling;
                    ++ref;
                }
            } else if (size < 20000) {
                while (*ref) {
                    rect.setXYWH(i, mmPosOffset, mmScaling * xScaling, h);
                    switch ((unsigned int)*ref) {
                        case 65: canvas->drawRect(rect, theme.fcA); break;
                        case 67: canvas->drawRect(rect, theme.fcC); break;
                        case 71: canvas->drawRect(rect, theme.fcG); break;
                        case 78: canvas->drawRect(rect, theme.fcN); break;
                        case 84: canvas->drawRect(rect, theme.fcT); break;
                        case 97: canvas->drawRect(rect, theme.fcA); break;
                        case 99: canvas->drawRect(rect, theme.fcC); break;
                        case 103: canvas->drawRect(rect, theme.fcG); break;
                        case 110: canvas->drawRect(rect, theme.fcN); break;
                        case 116: canvas->drawRect(rect, theme.fcT); break;
                    }
                    i += xScaling;
                    ++ref;
                }
            }
            index += 1;
        }
    }

    void drawBorders(const Themes::IniOptions &opts, float fb_width, float fb_height,
                 SkCanvas *canvas, size_t nregions, size_t nbams, float trackY, float covY) {
        SkPath path;
        float refSpace = fb_height * 0.02;
        if (nregions > 1) {
            float x = fb_width / nregions;
            float step = x;
            path.reset();
            for (int i=0; i < (int)nregions - 1; ++i) {
                path.moveTo(x, 0);
                path.lineTo(x, fb_height);
                x += step;
            }
            canvas->drawPath(path, opts.theme.lcLightJoins);
        }
        if (nbams > 1) {
            float y = trackY + covY;
            float step = y;
            y += refSpace;
            path.reset();
            for (int i=0; i<(int)nbams - 1; ++i) {
                path.moveTo(0, y);
                path.lineTo(fb_width, y);
                y += step;
            }
            canvas->drawPath(path, opts.theme.lcLightJoins);
        }
    }

    void drawLabel(const Themes::IniOptions &opts, SkCanvas *canvas, SkRect &rect, Utils::Label &label, Themes::Fonts &fonts,
                   const ankerl::unordered_dense::set<std::string> &seenLabels, const std::vector<std::string> &srtLabels) {

        float pad = 2;
        std::string cur = label.current();
        if (cur.empty()) {
            return;
        }
        sk_sp<SkTextBlob> blob = SkTextBlob::MakeFromString(cur.c_str(), fonts.overlay);
        float wl = fonts.overlayWidth * (cur.size() + 1);

        auto it = std::find(srtLabels.begin(), srtLabels.end(), cur);
        int idx;
        if (it != srtLabels.end()) {
            idx = it - srtLabels.begin();
        } else {
            idx = label.i + srtLabels.size();
        }

        float step, start;
        step = -1;
        start = 1;

        float value = start;
        for (int i=0; i < idx; i++) {
            value += step;
            step *= -1;
            step = step * 0.5;
        }

        SkRect bg;
        float x = rect.left() + pad;

        SkPaint p;
        int v;
        if (opts.theme.name == "igv") {
            v = 255 - (int)(value * 255);
        } else {
            v = (int)(value * 255);
        }
        p.setARGB(255, v, v, v);

        if ((wl + pad) > (rect.width() * 0.5)) {
            bg.setXYWH(x + pad, rect.bottom() - fonts.overlayHeight - pad - pad - pad - pad,  fonts.overlayHeight, fonts.overlayHeight);
            canvas->drawRoundRect(bg,  fonts.overlayHeight, fonts.overlayHeight, p);
            canvas->drawRoundRect(bg,  fonts.overlayHeight, fonts.overlayHeight, opts.theme.lcLabel);
        } else {
            bg.setXYWH(x + pad, rect.bottom() - fonts.overlayHeight - pad - pad - pad - pad,  wl + pad, fonts.overlayHeight + pad + pad);
            canvas->drawRoundRect(bg,  5, 5, p);
            canvas->drawRoundRect(bg,  5, 5, opts.theme.lcLabel);

            if (opts.theme.name == "igv") {
                if (v == 0) {
                    canvas->drawTextBlob(blob, x + pad + pad, bg.bottom() - pad - pad, opts.theme.tcBackground);
                } else {
                    canvas->drawTextBlob(blob, x + pad + pad, bg.bottom() - pad - pad, opts.theme.tcDel);
                }
            } else {
                if (v == 255) {
                    canvas->drawTextBlob(blob, x + pad + pad, bg.bottom() - pad - pad, opts.theme.tcBackground);
                } else {
                    canvas->drawTextBlob(blob, x + pad + pad, bg.bottom() - pad - pad, opts.theme.tcDel);
                }
            }
        }

        if (label.i != label.ori_i) {
            canvas->drawRect(rect, opts.theme.lcJoins);
        }
    }


    void drawTrackBlock(int start, int stop, std::string &rid, const Utils::Region &rgn, SkRect &rect, SkPath &path, float padX, float padY,
                        float y, float h, float stepX, float gap, float gap2, float xScaling, float t,
                        Themes::IniOptions &opts, SkCanvas *canvas, const Themes::Fonts &fonts,
                        bool add_text, bool add_rect, bool v_line) {
        float x = 0;
        float w, textW;
        if (start < rgn.start && stop >= rgn.end) { // track spans whole region
            rect.setXYWH(padX, y + padY - h, stepX - gap2, h);
            if (add_rect) {
                canvas->drawRect(rect, opts.theme.fcTrack);
            }
            w = (float)(stop - rgn.start) * xScaling;
            if (add_text && w > t) {
                textW = fonts.overlayWidth * ((float)rid.size() + 1);
                if (rect.left() + textW < padX + stepX - gap2 - gap2) {
                    rect.setXYWH(padX, y + (h/2) + padY, textW, h*2);
                    canvas->drawRect(rect, opts.theme.bgPaint);
                    if (2*h > fonts.overlayHeight) {
                        sk_sp<SkTextBlob> blob = SkTextBlob::MakeFromString(rid.c_str(), fonts.overlay);
                        canvas->drawTextBlob(blob, rect.left(), rect.bottom(), opts.theme.tcDel);
                    }
                }
            }
        } else if (start < rgn.start && stop >= rgn.start) {  // overhands left side
            w = (float)(stop - rgn.start) * xScaling;
            x = 0;
            rect.setXYWH(padX, y + padY - h, w, h);
            if (add_rect) {
                canvas->drawRect(rect, opts.theme.fcTrack);
            }
            if (add_text && w > t) {
                textW = fonts.overlayWidth * ((float)rid.size() + 1);
                if (rect.left() + textW < padX + stepX - gap2) {
                    rect.setXYWH(padX, y + (h/2) + padY, textW, h*2);
                    canvas->drawRect(rect, opts.theme.bgPaint);
                    if (2*h > fonts.overlayHeight) {
                        sk_sp<SkTextBlob> blob = SkTextBlob::MakeFromString(rid.c_str(), fonts.overlay);
                        canvas->drawTextBlob(blob, rect.left(), rect.bottom(), opts.theme.tcDel);
                    }
                }
            }
        } else if (start < rgn.end && stop > rgn.end) { // overhangs rhs
            x = (float)(start - rgn.start) * xScaling;
            w = (float)(rgn.end - start) * xScaling;
            rect.setXYWH(x + padX, y + padY - h, w, h);
            if (add_rect) {
                canvas->drawRect(rect, opts.theme.fcTrack);
            }
            if (add_text && w > t) {
                textW = fonts.overlayWidth * ((float)rid.size() + 1);
                if (rect.left() + textW < padX + stepX - gap2) {
                    rect.setXYWH(x + padX, y + (h/2) + padY, textW, h*2);
                    canvas->drawRect(rect, opts.theme.bgPaint);
                    if (2*h > fonts.overlayHeight) {
                        sk_sp<SkTextBlob> blob = SkTextBlob::MakeFromString(rid.c_str(), fonts.overlay);
                        canvas->drawTextBlob(blob, rect.left(), rect.bottom(), opts.theme.tcDel);
                    }
                }
            }
        } else if (start >= rgn.start && stop < rgn.end) { // all within view
            x = (float)(start - rgn.start) * xScaling;
            w = (float)(stop - start) * xScaling;
            rect.setXYWH(x + padX, y + padY - h, w, h);
            if (add_rect) {
                canvas->drawRect(rect, opts.theme.fcTrack);
            }
            if (add_text && w > t) {
                textW = fonts.overlayWidth * ((float)rid.size() + 1);
                if (rect.left() + textW < padX + stepX - gap2 - gap2) {
                    rect.setXYWH(x + padX, y + (h/2) + padY, textW, h*2);
                    canvas->drawRect(rect, opts.theme.bgPaint);
                    if (2*h > fonts.overlayHeight) {
                        sk_sp<SkTextBlob> blob = SkTextBlob::MakeFromString(rid.c_str(), fonts.overlay);
                        canvas->drawTextBlob(blob, rect.left(), rect.bottom(), opts.theme.tcDel);
                    }
                }
            }
        }
        if (v_line && x != 0) {
            path.moveTo(x + padX, y + padY - h);
            path.lineTo(x + padX, y + (h/2) + padY);
            canvas->drawPath(path, opts.theme.lcJoins);
        }
    }

    void drawTracks(Themes::IniOptions &opts, float fb_width, float fb_height,
                     SkCanvas *canvas, float totalTabixY, float tabixY, std::vector<HGW::GwTrack> &tracks,
                     const std::vector<Utils::Region> &regions, const Themes::Fonts &fonts, float gap) {

        if (tracks.empty() || regions.empty()) {
            return;
        }
        float gap2 = 2*gap;
        float padX = gap;
        float padY = 0;

        float stepX = fb_width / regions.size();
        float stepY = totalTabixY / tracks.size();
        float y = fb_height - totalTabixY;
        float h = (float)stepY * 0.2;
        float h2 = h * 0.5;
        float h4 = h2 * 0.5;
        float t = (float)0.005 * fb_width;
        SkRect rect{};
        SkPath path{};
        SkPath path2{};

        opts.theme.lcLightJoins.setAntiAlias(true);
        for (auto &rgn : regions) {
            bool any_text = (rgn.end - rgn.start) < 500000;
            float xScaling = (stepX - gap2) / (float)(rgn.end - rgn.start);
            for (auto & trk : tracks) {
                trk.fetch(&rgn);
                while (true) {
                    trk.next();
                    if (trk.done) {
                        break;
                    }
                    // check for big bed. BED_IDX will already be split, BED_NOI is split here
                    if (trk.kind == HGW::BED_NOI) {
                        trk.parts.clear();
                        Utils::split(trk.variantString, '\t', trk.parts);
                    }
                    if (!trk.parts.empty() && trk.parts.size() >= 12) {
                        std::vector<std::string> lens, starts;
                        Utils::split(trk.parts[10], ',', lens);
                        Utils::split(trk.parts[11], ',', starts);
                        if (any_text) {
                            drawTrackBlock(trk.start, trk.stop, trk.rid, rgn, rect, path, padX, padY, y, h, stepX, gap, gap2, xScaling, t,
                                           opts, canvas, fonts, true, false, false);
                        }
                        int last_end = 0;
                        if (starts.size() == lens.size()) {
                            int target = (int)lens.size();
                            int gene_start = trk.start;
                            int stranded = (trk.parts[5] == "+") ? 1 : (trk.parts[5] == "-") ? -1 : 0;
                            int thickStart = std::stoi(trk.parts[6]);
                            int thickEnd = (std::stoi(trk.parts[7]));
                            thickEnd = (thickEnd == thickStart) ? trk.stop : thickEnd;
                            bool add_line = true;  // vertical line at start of interval
                            for (int i=0; i < target; ++i) {
                                int s, e;
                                try {
                                    s = gene_start + std::stoi(starts[i]);
                                    e = s + std::stoi(lens[i]);
                                } catch (...) {
                                    std::cerr << "Error: problem parsing big bed, pos was: " << trk.parts[0] << " " << trk.parts[1] << std::endl;
                                    break;
                                }
                                if (s > rgn.end && last_end > rgn.end) {
                                    break;
                                } else if (e < rgn.start) {
                                    last_end = e;
                                    continue;
                                }
                                if (s >= thickStart && e <= thickEnd) { // block is all thick
                                    drawTrackBlock(s, e, trk.rid, rgn, rect, path, padX, padY, y, h, stepX, gap, gap2, xScaling, t,
                                                   opts, canvas, fonts, false, true, add_line);
                                } else if (e < thickStart || s > thickEnd) {  // block is all thin
                                    drawTrackBlock(s, e, trk.rid, rgn, rect, path, padX, padY, y - h4, h2, stepX, gap, gap2, xScaling, t,
                                                   opts, canvas, fonts, false, true, add_line);

                                } else {  // left or right is thin
                                    if (s < thickStart) {  // left small block
                                        drawTrackBlock(s, thickStart, trk.rid, rgn, rect, path, padX, padY, y - h4, h2, stepX, gap, gap2, xScaling, t,
                                                       opts, canvas, fonts, false, true, add_line);
                                        add_line = false;

                                    }
                                    if (e > thickEnd) {  // right small block
                                        drawTrackBlock(thickEnd, e, trk.rid, rgn, rect, path, padX, padY, y - h4, h2, stepX, gap, gap2, xScaling, t,
                                                       opts, canvas, fonts, false, true, add_line);
                                        add_line = false;
                                    }
                                    drawTrackBlock(std::max(s, thickStart), std::min(e, thickEnd), trk.rid, rgn, rect, path, padX, padY, y, h, stepX, gap, gap2, xScaling, t,
                                                   opts, canvas, fonts, false, true, add_line);

                                }

                                float x, yy, w;
                                yy = y + padY - (h/2);
                                if (i > 0) {  // add arrows
                                    x = ((float)(std::max(last_end, rgn.start) - rgn.start) * xScaling) + padX;
                                    w = ((float)(std::min(s, rgn.end) - rgn.start) * xScaling) + padX;
                                    if (w > 0) {
                                        path2.reset();
                                        path2.moveTo(x, yy);
                                        path2.lineTo(w, yy);
                                        canvas->drawPath(path2, opts.theme.fcTrack);
                                        if (stranded != 0 && w - x > 50) {
                                            while (x + 50 < w) {
                                                x += 50;
                                                path2.reset();
                                                if (stranded == 1) {
                                                    path2.moveTo(x, yy);
                                                    path2.lineTo(x-6, yy + 6);
                                                    path2.moveTo(x, yy);
                                                    path2.lineTo(x-6, yy - 6);
                                                } else {
                                                    path2.moveTo(x, yy);
                                                    path2.lineTo(x+6, yy + 6);
                                                    path2.moveTo(x, yy);
                                                    path2.lineTo(x+6, yy - 6);
                                                }
                                                canvas->drawPath(path2, opts.theme.fcTrack);
                                            }
                                        }
                                    }

                                }
                                if (stranded != 0) {
                                    x = ((float)(std::max(s, rgn.start) - rgn.start) * xScaling) + padX;
                                    w = ((float)(std::min(e, rgn.end) - rgn.start) * xScaling) + padX;
                                    if (w - x > 50) {
                                        while (x + 50 < w) {
                                            x += 50;
                                            path2.reset();
                                            if (stranded == 1) {
                                                path2.moveTo(x, yy);
                                                path2.lineTo(x-4, yy + 4);
                                                path2.moveTo(x, yy);
                                                path2.lineTo(x-4, yy - 4);
                                            } else {
                                                path2.moveTo(x, yy);
                                                path2.lineTo(x+4, yy + 4);
                                                path2.moveTo(x, yy);
                                                path2.lineTo(x+4, yy - 4);
                                            }
                                            canvas->drawPath(path2, opts.theme.bgPaint);
                                        }
                                    }
                                }
                                add_line = false;
                                last_end = e;
                            }
                        } else {
                            drawTrackBlock(trk.start, trk.stop, trk.rid, rgn, rect, path, padX, padY, y, h, stepX, gap, gap2, xScaling, t,
                                           opts, canvas, fonts, any_text, true, true);
                        }
                    } else {
                        drawTrackBlock(trk.start, trk.stop, trk.rid, rgn, rect, path, padX, padY, y, h, stepX, gap, gap2, xScaling, t,
                                       opts, canvas, fonts, any_text, true, true);
                    }
                }
                padY += stepY;
            }
            padY = 0;
            padX += stepX;
        }
        opts.theme.lcLightJoins.setAntiAlias(false);
    }

    void drawChromLocation(const Themes::IniOptions &opts, const std::vector<Segs::ReadCollection> &collections, SkCanvas* canvas,
                           const faidx_t* fai, std::vector<sam_hdr_t* > &headers, size_t nRegions, float fb_width, float fb_height, float monitorScale) {
        SkPaint paint, line;
        paint.setColor(SK_ColorRED);
        paint.setStrokeWidth(3);
        paint.setStyle(SkPaint::kStroke_Style);
        line.setColor((opts.theme_str == "dark") ? SK_ColorWHITE : SK_ColorBLACK);
        line.setStrokeWidth(monitorScale);
        line.setStyle(SkPaint::kStroke_Style);
        SkRect rect{};
        SkPath path{};
        auto yh = (float)(fb_height * 0.015);
        float rowHeight = (float)fb_height / (float)headers.size();
        float colWidth = (float)fb_width / (float)nRegions;
        float gap = 50;
        float gap2 = 2*gap;
        float drawWidth = colWidth - gap2;
        if (drawWidth < 0) {
            return;
        }
        for (auto &cl: collections) {
            if (cl.bamIdx + 1 != (int)headers.size()) {
                continue;
            }
            auto length = (float) faidx_seq_len(fai, cl.region.chrom.c_str());
            float s = (float)cl.region.start / length;
            float e = (float)cl.region.end / length;
            float w = (e - s) * drawWidth;
            if (w < 3 ) {
                w = 3;
            }
            float yp = ((cl.bamIdx + 1) * rowHeight) - yh;
            float xp = (cl.regionIdx * colWidth) + gap;
            rect.setXYWH(xp + (s * drawWidth),
                         yp - 4,
                         w,
                         yh);
            path.reset();
            path.moveTo(xp, ((cl.bamIdx + 1) * rowHeight) - (yh * 0.5) - 4);
            path.lineTo(xp + drawWidth, ((cl.bamIdx + 1) * rowHeight) - (yh * 0.5) - 4);
            canvas->drawPath(path, line);
            canvas->drawRect(rect, paint);
        }
    }
}