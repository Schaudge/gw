//
// Created by Kez Cleal on 25/07/2022.
//

#pragma once

#include <iostream>
#ifdef __APPLE__
    #include <OpenGL/gl.h>
#endif

#include "htslib/faidx.h"
#include "htslib/hfile.h"
#include "htslib/hts.h"
#include "htslib/sam.h"


#include <GLFW/glfw3.h>
#include <string>
#include <utility>
#include <vector>

#include "hts_funcs.h"
#include "../inc/robin_hood.h"
#include "utils.h"
#include "segments.h"
#include "themes.h"

#define SK_GL
#include "include/gpu/GrBackendSurface.h"
#include "include/gpu/GrDirectContext.h"
#include "include/gpu/gl/GrGLInterface.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkSurface.h"


namespace Manager {

    /*
     * Deals with window functions
     */
    class SkiaWindow {
    public:
        SkiaWindow() {};
        ~SkiaWindow();

        GLFWwindow* window;

        void init(int width, int height);

        int pollWindow(SkCanvas* canvas, GrDirectContext* sContext);

    };

    /*
     * Deals with managing genomic data
     */
    class GwPlot {
    public:
        GwPlot(std::string reference, std::vector<std::string>& bams, Themes::IniOptions& opts, std::vector<Utils::Region>& regions);
        ~GwPlot();

        bool init;
        bool redraw;
        bool processed;

        std::string reference;

        std::vector<std::string> bam_paths;
        std::vector<htsFile* > bams;
        std::vector<sam_hdr_t* > headers;
        std::vector<hts_idx_t* > indexes;
        std::vector<Utils::Region> regions;

        Themes::IniOptions opts;
        faidx_t* fai;
        SkiaWindow window;

        int plotToScreen(SkCanvas* canvas, GrDirectContext* sContext);

    private:

        float totalCovY, covY, totalTabixY, tabixY, trackY;

        std::vector< robin_hood::unordered_map< const char *, std::vector<int> >> linked;
        std::vector<Segs::ReadCollection> all_segs;

        void drawScreen(SkCanvas* canvas, GrDirectContext* sContext);

        void setYspace();

        void process_sam(SkCanvas* canvas);
    };


}