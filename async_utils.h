#pragma once

#include <iostream>
#include "bulk_utils.h"

namespace otus_hw9{
    using  std::istream;
    struct Options : public otus_hw7::Options
    {
        using BaseCls_t = otus_hw7::Options;
        size_t thread_count;
        Options() : thread_count(2) {}
        Options(size_t cmd_bulk_sz, istream* istrm, size_t thread_cnt) : BaseCls_t(cmd_bulk_sz, istrm), thread_count(thread_cnt) {}
        virtual BaseCls_t& add_options(otus_hw7::po::options_description& desc) override;        
    };
};