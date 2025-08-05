#include <iostream>

#include "async_utils.h"

namespace otus_hw9{

    namespace{
        constexpr const char* const OPTION_NAME_THREAD_COUNT = "thread_count"; 
    }

    Options::BaseCls_t& Options::add_options(otus_hw7::po::options_description& desc)
    {
        BaseCls_t::add_options(desc);
        auto check_size = [](const size_t& sz) 
                          { 
                            if( sz < 1 ) throw otus_hw7::po::invalid_option_value(OPTION_NAME_THREAD_COUNT); 
                          };
        desc.add_options()
            (OPTION_NAME_THREAD_COUNT, otus_hw7::po::value<size_t>(&thread_count)->notifier(check_size), "Число потоков для обработки");
        return *this;
    }        
};